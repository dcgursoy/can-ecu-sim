#include <cstdint>

#include <gtest/gtest.h>

#include "ecu/can_error_sm.hpp"

namespace {

using ecu::BusOffPolicy;
using ecu::CanErrorStateMachine;
using Action = CanErrorStateMachine::Action;
using Phase = CanErrorStateMachine::Phase;
using proto::CanState;

constexpr BusOffPolicy kPolicy{100, 1600, 500, 5, 10000};

TEST(CanErrorStateMachine, PassiveIsCountedAndStillTransmits)
{
    CanErrorStateMachine sm(kPolicy);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_TRUE(sm.tx_allowed());
    sm.on_controller_state(CanState::kErrorPassive, 1000);
    EXPECT_EQ(sm.phase(), Phase::kPassive);
    EXPECT_TRUE(sm.tx_allowed());
    EXPECT_EQ(sm.counters().passive_events, 1U);
    sm.on_controller_state(CanState::kErrorPassive, 1001);  // no double count
    EXPECT_EQ(sm.counters().passive_events, 1U);
    sm.on_controller_state(CanState::kErrorActive, 1002);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_EQ(sm.tick(1003), Action::kNone);
}

TEST(CanErrorStateMachine, BusOffBacksOffThenRecovers)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kBusOff, 1000);
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff);
    EXPECT_FALSE(sm.tx_allowed());
    EXPECT_TRUE(sm.bus_off());
    EXPECT_EQ(sm.counters().bus_off_events, 1U);
    EXPECT_EQ(sm.current_backoff_ms(), 100U);

    EXPECT_EQ(sm.tick(1050), Action::kNone);
    EXPECT_EQ(sm.tick(1099), Action::kNone);
    EXPECT_EQ(sm.tick(1100), Action::kStartRecovery);
    EXPECT_EQ(sm.phase(), Phase::kBusOffRecovering);
    EXPECT_EQ(sm.counters().recovery_attempts, 1U);
    sm.on_recovery_request_result(true, 1100);
    EXPECT_EQ(sm.tick(1200), Action::kNone) << "no second request while recovering";

    sm.on_controller_state(CanState::kErrorActive, 1103);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_TRUE(sm.tx_allowed());
    EXPECT_EQ(sm.counters().recoveries_ok, 1U);
    EXPECT_EQ(sm.counters().last_recovery_duration_ms, 103);
    EXPECT_EQ(sm.attempt(), 0U) << "a successful recovery keeps the attempt count for the next backoff";

    // A second bus-off long after a recovery that held is a fresh event.
    sm.on_controller_state(CanState::kBusOff, 1103 + kPolicy.stable_ms);
    EXPECT_EQ(sm.current_backoff_ms(), 100U);
    EXPECT_EQ(sm.counters().recovery_failures, 0U);
}

TEST(CanErrorStateMachine, RepeatedFailuresDoubleTheBackoffAndEventuallyLatchFailed)
{
    CanErrorStateMachine sm(kPolicy);
    std::int64_t t = 0;
    const std::uint32_t expected_backoff[] = {100, 200, 400, 800};
    sm.on_controller_state(CanState::kBusOff, t);
    for (const std::uint32_t backoff : expected_backoff) {
        EXPECT_EQ(sm.current_backoff_ms(), backoff);
        t += backoff;
        EXPECT_EQ(sm.tick(t - 1), Action::kNone);
        EXPECT_EQ(sm.tick(t), Action::kStartRecovery);
        t += kPolicy.recovery_timeout_ms + 1;  // driver never reports error-active
        EXPECT_EQ(sm.tick(t), Action::kNone);
        EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff) << "back to backoff after a failed attempt";
    }
    EXPECT_EQ(sm.counters().recovery_failures, 4U);
    EXPECT_EQ(sm.attempt(), 4U);
    EXPECT_EQ(sm.current_backoff_ms(), 1600U);
    t += 1600;
    EXPECT_EQ(sm.tick(t), Action::kStartRecovery);  // fifth and last attempt
    sm.on_controller_state(CanState::kBusOff, t + 10);  // bus went off again during recovery
    EXPECT_EQ(sm.phase(), Phase::kFailed);
    EXPECT_TRUE(sm.failed());
    EXPECT_FALSE(sm.tx_allowed());
    EXPECT_EQ(sm.counters().recovery_failures, 5U);
    EXPECT_EQ(sm.counters().bus_off_events, 1U) << "re-entering bus-off during recovery is one event";

    // Latched: further driver reports do not revive it, reset() does.
    sm.on_controller_state(CanState::kErrorActive, t + 20);
    EXPECT_TRUE(sm.failed());
    sm.reset(t + 30);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_EQ(sm.attempt(), 0U);
}

TEST(CanErrorStateMachine, BackoffIsCappedAndForgottenAfterStablePeriod)
{
    CanErrorStateMachine sm(kPolicy);
    // Two failed attempts, then success: attempt counter stays at 2 until the bus is stable.
    sm.on_controller_state(CanState::kBusOff, 0);
    sm.tick(100);
    sm.tick(601);  // timeout -> attempt 1
    sm.tick(801);  // 200 ms backoff elapsed -> recovery 2
    sm.tick(1302); // timeout -> attempt 2
    EXPECT_EQ(sm.attempt(), 2U);
    sm.tick(1702); // 400 ms backoff -> recovery 3
    sm.on_controller_state(CanState::kErrorActive, 1710);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_EQ(sm.attempt(), 2U);
    sm.tick(1710 + 9999);
    EXPECT_EQ(sm.attempt(), 2U);
    sm.tick(1710 + 10000);
    EXPECT_EQ(sm.attempt(), 0U) << "stable for stable_ms forgets the history";

    // The next bus-off starts again at the initial backoff.
    sm.on_controller_state(CanState::kBusOff, 20000);
    EXPECT_EQ(sm.current_backoff_ms(), 100U);
}

// bxCAN raises its status interrupt only when an error flag is set, never when
// it clears, so the return to error-active after recovery arrives by polling.
TEST(CanErrorStateMachine, RecoveryCompletesThroughPollingWithoutAnEvent)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kBusOff, 0);
    EXPECT_EQ(sm.tick(100), Action::kStartRecovery);
    sm.poll_controller_state(CanState::kBusOff, 102);  // sequence still running: not a failure
    EXPECT_EQ(sm.phase(), Phase::kBusOffRecovering);
    EXPECT_EQ(sm.counters().recovery_failures, 0U);
    sm.poll_controller_state(CanState::kErrorActive, 105);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_TRUE(sm.tx_allowed());
    EXPECT_EQ(sm.counters().recoveries_ok, 1U);
    EXPECT_EQ(sm.counters().last_recovery_duration_ms, 105);
    sm.poll_controller_state(CanState::kErrorActive, 200);  // unchanged level: no effect
    EXPECT_EQ(sm.counters().recoveries_ok, 1U);
}

TEST(CanErrorStateMachine, PollingDeliversPassiveToActiveAndBusOffWithoutCallback)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kErrorPassive, 0);
    EXPECT_EQ(sm.phase(), Phase::kPassive);
    sm.poll_controller_state(CanState::kErrorActive, 50);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    sm.poll_controller_state(CanState::kBusOff, 60);  // a bus-off nobody reported
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff);
    EXPECT_EQ(sm.counters().bus_off_events, 1U);
    sm.poll_controller_state(CanState::kBusOff, 70);  // during backoff: ignored
    EXPECT_EQ(sm.counters().bus_off_events, 1U);
    sm.poll_controller_state(CanState::kStopped, 80);  // never acted on
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff);
}

TEST(CanErrorStateMachine, RepeatedErrorActiveReportsDoNotRestartTheStableWindow)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kBusOff, 0);
    sm.tick(100);
    sm.tick(601);  // failed attempt -> attempt 1
    sm.tick(801);
    sm.on_controller_state(CanState::kErrorActive, 805);
    EXPECT_EQ(sm.attempt(), 1U);
    // The HAL folds error-warning into error-active: repeated reports while active.
    sm.on_controller_state(CanState::kErrorActive, 5000);
    sm.on_controller_state(CanState::kErrorActive, 9000);
    sm.tick(805 + 10000);
    EXPECT_EQ(sm.attempt(), 0U) << "the window counts from the first error-active, not the last report";
}

TEST(CanErrorStateMachine, RecoveryThatDoesNotHoldCountsAsAFailedAttempt)
{
    CanErrorStateMachine sm(kPolicy);
    std::int64_t t = 0;
    // Bus-off -> recover -> active for a few ms -> bus-off again, repeatedly. The
    // first bus-off is a fresh event; every later one within stable_ms of the
    // previous recovery is a failed attempt, so the backoff doubles each time.
    const std::uint32_t expected_backoff[] = {100, 200, 400, 800, 1600};
    for (int cycle = 0; cycle < 5; ++cycle) {
        sm.on_controller_state(CanState::kBusOff, t);
        ASSERT_FALSE(sm.failed()) << "cycle " << cycle;
        const std::uint32_t backoff = sm.current_backoff_ms();
        EXPECT_EQ(backoff, expected_backoff[cycle]) << "backoff keeps doubling for a flapping node";
        t += backoff;
        EXPECT_EQ(sm.tick(t), Action::kStartRecovery);
        t += 3;
        sm.on_controller_state(CanState::kErrorActive, t);
        EXPECT_EQ(sm.phase(), Phase::kActive);
        t += 7;
    }
    EXPECT_EQ(sm.counters().recoveries_ok, 5U);
    EXPECT_EQ(sm.counters().recovery_failures, 4U);
    sm.on_controller_state(CanState::kBusOff, t);  // fifth failed attempt: exhausted
    EXPECT_TRUE(sm.failed());
    EXPECT_EQ(sm.counters().recovery_failures, 5U);
    EXPECT_EQ(sm.counters().bus_off_events, 6U);
}

TEST(CanErrorStateMachine, ResetWhileStillBusOffRestartsRecovery)
{
    CanErrorStateMachine sm(kPolicy);
    std::int64_t t = 0;
    sm.on_controller_state(CanState::kBusOff, t);
    for (int attempt = 0; attempt < 5; ++attempt) {
        t += sm.current_backoff_ms();
        sm.tick(t);
        t += kPolicy.recovery_timeout_ms + 1;
        sm.tick(t);
    }
    ASSERT_TRUE(sm.failed());
    EXPECT_EQ(sm.controller_state(), CanState::kBusOff);
    sm.reset(t);
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff) << "controller still bus-off: back to the first backoff";
    EXPECT_EQ(sm.current_backoff_ms(), 100U);
    EXPECT_FALSE(sm.tx_allowed());
    EXPECT_EQ(sm.tick(t + 100), Action::kStartRecovery);
    sm.poll_controller_state(CanState::kErrorActive, t + 104);
    EXPECT_EQ(sm.phase(), Phase::kActive);

    // A reset after the controller already came back by itself goes straight to active.
    CanErrorStateMachine sm2(kPolicy);
    std::int64_t u = 0;
    sm2.on_controller_state(CanState::kBusOff, u);
    while (!sm2.failed()) {
        u += 100000;
        sm2.tick(u);  // backoff long elapsed: starts an attempt
        u += 100000;
        sm2.tick(u);  // recovery timeout long elapsed: the attempt failed
    }
    ASSERT_TRUE(sm2.failed());
    sm2.on_controller_state(CanState::kErrorActive, u + 1);  // ignored while failed, but remembered
    sm2.reset(u + 2);
    EXPECT_EQ(sm2.phase(), Phase::kActive);
}

TEST(CanErrorStateMachine, BackoffIsCappedAtMaxBackoffMs)
{
    constexpr BusOffPolicy kCapped{100, 500, 500, 6, 10000};
    CanErrorStateMachine sm(kCapped);
    std::int64_t t = 0;
    const std::uint32_t expected_backoff[] = {100, 200, 400, 500, 500, 500};
    sm.on_controller_state(CanState::kBusOff, t);
    for (const std::uint32_t backoff : expected_backoff) {
        EXPECT_EQ(sm.current_backoff_ms(), backoff);
        t += backoff;
        EXPECT_EQ(sm.tick(t - 1), Action::kNone);
        EXPECT_EQ(sm.tick(t), Action::kStartRecovery);
        t += kCapped.recovery_timeout_ms + 1;  // driver never reports error-active
        EXPECT_EQ(sm.tick(t), Action::kNone);
    }
    EXPECT_EQ(sm.phase(), Phase::kFailed);
}

TEST(CanErrorStateMachine, RefusedRecoveryRequestCountsAsFailure)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kBusOff, 0);
    EXPECT_EQ(sm.tick(100), Action::kStartRecovery);
    sm.on_recovery_request_result(false, 100);
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff);
    EXPECT_EQ(sm.attempt(), 1U);
    EXPECT_EQ(sm.counters().recovery_failures, 1U);
}

// The simulated HAL (and some drivers) report a state through the callback and
// the supervisor polls the same state on its next tick: one bus-off must stay
// one event, and a bus-off reading during backoff, during the recovery
// sequence or after FAULT is the condition already being handled.
TEST(CanErrorStateMachine, BusOffReportedAgainWhileHandlingItIsOneEvent)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kBusOff, 0);
    sm.on_controller_state(CanState::kBusOff, 1);    // the callback fired twice
    sm.poll_controller_state(CanState::kBusOff, 5);  // and the poll sees it as well
    EXPECT_EQ(sm.counters().bus_off_events, 1U);
    EXPECT_EQ(sm.phase(), Phase::kBusOffBackoff);
    EXPECT_EQ(sm.current_backoff_ms(), 100U);
    EXPECT_EQ(sm.tick(100), Action::kStartRecovery);
    sm.poll_controller_state(CanState::kBusOff, 101);  // recovery sequence still running
    EXPECT_EQ(sm.phase(), Phase::kBusOffRecovering);
    EXPECT_EQ(sm.counters().recovery_failures, 0U);

    std::int64_t t = 101;
    while (!sm.failed()) {
        t += 100000;
        sm.tick(t);  // recovery timeout long elapsed: the attempt failed
        t += 100000;
        sm.tick(t);  // backoff long elapsed: next attempt
    }
    sm.on_controller_state(CanState::kErrorActive, t + 1);  // ignored while failed, but remembered
    sm.poll_controller_state(CanState::kBusOff, t + 2);     // a reading that differs from the last report
    sm.on_controller_state(CanState::kBusOff, t + 3);
    EXPECT_TRUE(sm.failed());
    EXPECT_EQ(sm.counters().bus_off_events, 1U) << "nothing is counted after FAULT";
}

TEST(CanErrorStateMachine, TickWhilePassiveOrNotYetStableDoesNothing)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kErrorPassive, 0);
    EXPECT_EQ(sm.tick(50), Action::kNone);
    EXPECT_EQ(sm.phase(), Phase::kPassive);
    EXPECT_TRUE(sm.tx_allowed());
    // One failed attempt, then a recovery: the attempt count survives until stable_ms.
    sm.on_controller_state(CanState::kBusOff, 100);
    sm.tick(200);  // first attempt
    sm.tick(701);  // its recovery timed out: attempt 1, 200 ms backoff
    sm.tick(901);  // second attempt
    sm.on_controller_state(CanState::kErrorActive, 905);
    EXPECT_EQ(sm.attempt(), 1U);
    EXPECT_EQ(sm.tick(905 + 9999), Action::kNone);
    EXPECT_EQ(sm.attempt(), 1U) << "not stable for stable_ms yet";
    // reset() outside FAULT only forgets the attempt history.
    sm.reset(906 + 9999);
    EXPECT_EQ(sm.attempt(), 0U);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_EQ(sm.current_backoff_ms(), 100U);
}

// A policy the compiler cannot see (the firmware's comes from Kconfig; on a
// target without constant initialisation the constructor runs at boot).
TEST(CanErrorStateMachine, PolicyFromRuntimeValues)
{
    const volatile std::uint32_t initial = 250;
    const BusOffPolicy policy{initial, 1000, 50, 2, 1000};
    CanErrorStateMachine sm(policy);
    EXPECT_EQ(sm.policy().initial_backoff_ms, 250U);
    EXPECT_EQ(sm.current_backoff_ms(), 250U);
    sm.on_controller_state(CanState::kBusOff, 0);
    EXPECT_EQ(sm.tick(249), Action::kNone);
    EXPECT_EQ(sm.tick(250), Action::kStartRecovery);
    sm.tick(301);  // timed out: the second and last attempt follows after 500 ms
    EXPECT_EQ(sm.current_backoff_ms(), 500U);
    EXPECT_EQ(sm.tick(801), Action::kStartRecovery);
    sm.tick(852);
    EXPECT_TRUE(sm.failed());
    EXPECT_EQ(sm.counters().recovery_attempts, 2U);
}

TEST(CanErrorStateMachine, PhaseNamesAndStoppedIsIgnored)
{
    CanErrorStateMachine sm(kPolicy);
    sm.on_controller_state(CanState::kStopped, 0);
    EXPECT_EQ(sm.phase(), Phase::kActive);
    EXPECT_EQ(sm.controller_state(), CanState::kStopped);
    EXPECT_STREQ(CanErrorStateMachine::to_string(Phase::kActive), "active");
    EXPECT_STREQ(CanErrorStateMachine::to_string(Phase::kPassive), "passive");
    EXPECT_STREQ(CanErrorStateMachine::to_string(Phase::kBusOffBackoff), "bus_off_backoff");
    EXPECT_STREQ(CanErrorStateMachine::to_string(Phase::kBusOffRecovering), "bus_off_recovering");
    EXPECT_STREQ(CanErrorStateMachine::to_string(Phase::kFailed), "failed");
    EXPECT_STREQ(CanErrorStateMachine::to_string(static_cast<Phase>(42)), "unknown");  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange): the out-of-table value is the point
}

}  // namespace

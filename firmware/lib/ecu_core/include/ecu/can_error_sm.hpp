// CAN error handling policy on top of the controller states the driver reports
// (error-active, error-passive, bus-off). The hardware (or the simulated
// controller behind the HAL hook) owns the counters and the state; this class
// owns what the ECU does about it:
//
//   error-active      transmit normally
//   error-passive     keep transmitting (the controller enforces the suspend
//                     transmission penalty itself), count the event
//   bus-off           stop transmitting; after a backoff, ask the driver to
//                     recover; double the backoff on every attempt up to a cap;
//                     after max_attempts consecutive failures latch "failed"
//                     and let the node state machine enter FAULT
//   recovered         when the controller is error-active again; attempts are
//                     forgotten once the controller has stayed error-active for
//                     stable_ms without interruption (time spent error-passive
//                     does not count, and a passive episode restarts the window)
//
// Two input paths feed the machine, because drivers differ in what they
// report. on_controller_state() is edge-triggered: a state-change callback. The
// bxCAN driver raises that callback only when a worse state is entered (its
// status interrupt fires when an error flag is set, never when it clears), so
// the return to error-active after recovery, and from passive to active, would
// never arrive. poll_controller_state() is therefore level-triggered: the
// supervisor reads the controller state every tick and feeds it here, and the
// method only reacts to changes that the callback path cannot deliver.
//
// Why back off at all: a node that goes bus-off because it is the one
// corrupting the bus (bad termination, wrong bit rate) and reconnects
// immediately will take the bus down again; spacing the attempts out gives the
// other nodes bandwidth and gives a diagnostic a chance to see the pattern. A
// recovery that does not hold (bus-off again before stable_ms) therefore counts
// as a failed attempt too, so the backoff keeps doubling and a node that keeps
// taking the bus down ends in FAULT instead of flapping forever.
// Why a cap on attempts: a permanent fault must end in a defined state, not an
// infinite reconnect loop.
#pragma once

#include <cstdint>

#include "protocol/messages.hpp"

namespace ecu {

struct BusOffPolicy {
    std::uint32_t initial_backoff_ms = 100;
    std::uint32_t max_backoff_ms = 1600;
    std::uint32_t recovery_timeout_ms = 500;  ///< time allowed for the controller to report error-active
    std::uint8_t max_attempts = 5;
    std::uint32_t stable_ms = 10000;          ///< error-active this long forgets past attempts
};

class CanErrorStateMachine {
public:
    enum class Phase : std::uint8_t {
        kActive,
        kPassive,
        kBusOffBackoff,     ///< waiting for the backoff to elapse
        kBusOffRecovering,  ///< recovery requested, waiting for error-active
        kFailed,            ///< attempts exhausted, latched until reset()
    };

    enum class Action : std::uint8_t { kNone, kStartRecovery };

    struct Counters {
        std::uint32_t bus_off_events = 0;
        std::uint32_t passive_events = 0;
        std::uint32_t recovery_attempts = 0;
        std::uint32_t recoveries_ok = 0;
        std::uint32_t recovery_failures = 0;
        std::int64_t last_bus_off_ms = -1;
        std::int64_t last_recovery_ms = -1;
        std::int64_t last_recovery_duration_ms = -1;  ///< bus-off to error-active
    };

    constexpr explicit CanErrorStateMachine(BusOffPolicy policy) : policy_(policy) {}

    /// Edge-triggered input: the driver (or simulated controller) reported a new state.
    constexpr void on_controller_state(proto::CanState state, std::int64_t now_ms)
    {
        last_state_ = state;
        if (phase_ == Phase::kFailed) {
            return;  // latched; a reset() clears it
        }
        switch (state) {
        case proto::CanState::kBusOff:
            if (phase_ == Phase::kBusOffBackoff) {
                break;  // duplicate report while already waiting
            }
            if (phase_ == Phase::kBusOffRecovering) {
                recovery_failed(now_ms);  // went off again before becoming active
                break;
            }
            ++counters_.bus_off_events;
            counters_.last_bus_off_ms = now_ms;
            if (counters_.last_recovery_ms >= 0 && now_ms - counters_.last_recovery_ms < policy_.stable_ms) {
                // The previous recovery did not hold: count it as a failed attempt so
                // the backoff doubles and a flapping node eventually latches FAULT.
                recovery_failed(now_ms);
            } else {
                begin_backoff(now_ms);
            }
            break;
        case proto::CanState::kErrorPassive:
            if (phase_ == Phase::kActive) {
                ++counters_.passive_events;
                phase_ = Phase::kPassive;
            }
            break;
        case proto::CanState::kErrorActive:
            if (phase_ == Phase::kBusOffRecovering) {
                ++counters_.recoveries_ok;
                counters_.last_recovery_ms = now_ms;
                counters_.last_recovery_duration_ms = now_ms - counters_.last_bus_off_ms;
            }
            // Idempotent: the HAL folds the driver's error-warning level into
            // error-active, so a repeated report must not restart the stable window.
            if (phase_ == Phase::kPassive || phase_ == Phase::kBusOffRecovering) {
                phase_ = Phase::kActive;
                active_since_ms_ = now_ms;
            }
            break;
        case proto::CanState::kStopped:
            break;
        }
    }

    /// Level-triggered input: the controller state as read right now. Reacts only
    /// to the improvements a status-change interrupt never signals (passive ->
    /// active, bus-off -> active after recovery) and to a bus-off that arrived
    /// without a callback. A bus-off reading during backoff or recovery is the
    /// condition already being handled (the recovery sequence takes 128 x 11 bit
    /// times) and is ignored.
    constexpr void poll_controller_state(proto::CanState state, std::int64_t now_ms)
    {
        if (state == last_state_ || state == proto::CanState::kStopped) {
            return;
        }
        if (state == proto::CanState::kBusOff &&
            (phase_ == Phase::kBusOffBackoff || phase_ == Phase::kBusOffRecovering || phase_ == Phase::kFailed)) {
            return;
        }
        on_controller_state(state, now_ms);
    }

    /// Periodic call; returns kStartRecovery when the HAL should call recover().
    constexpr Action tick(std::int64_t now_ms)
    {
        switch (phase_) {
        case Phase::kBusOffBackoff:
            if (now_ms >= recover_at_ms_) {
                phase_ = Phase::kBusOffRecovering;
                recovery_deadline_ms_ = now_ms + policy_.recovery_timeout_ms;
                ++counters_.recovery_attempts;
                return Action::kStartRecovery;
            }
            break;
        case Phase::kBusOffRecovering:
            if (now_ms > recovery_deadline_ms_) {
                recovery_failed(now_ms);
            }
            break;
        case Phase::kActive:
            if (attempt_ != 0U && active_since_ms_ >= 0 && now_ms - active_since_ms_ >= policy_.stable_ms) {
                attempt_ = 0;  // controller has been healthy long enough to forget the history
            }
            break;
        case Phase::kPassive:
        case Phase::kFailed:
            break;
        }
        return Action::kNone;
    }

    /// Result of the HAL's recover() call (false: the driver refused).
    constexpr void on_recovery_request_result(bool accepted, std::int64_t now_ms)
    {
        if (!accepted && phase_ == Phase::kBusOffRecovering) {
            recovery_failed(now_ms);
        }
    }

    /// Clear a latched failure (test command or operator action). If the
    /// controller is still bus-off the backoff/recovery cycle starts again from
    /// the first attempt: a driver that reports only changes will never repeat
    /// the BUS_OFF event for us.
    constexpr void reset(std::int64_t now_ms)
    {
        attempt_ = 0;
        if (phase_ != Phase::kFailed) {
            return;
        }
        if (last_state_ == proto::CanState::kBusOff) {
            begin_backoff(now_ms);
        } else {
            phase_ = last_state_ == proto::CanState::kErrorPassive ? Phase::kPassive : Phase::kActive;
            active_since_ms_ = now_ms;
        }
    }

    constexpr bool tx_allowed() const { return phase_ == Phase::kActive || phase_ == Phase::kPassive; }
    constexpr bool failed() const { return phase_ == Phase::kFailed; }
    constexpr bool bus_off() const
    {
        return phase_ == Phase::kBusOffBackoff || phase_ == Phase::kBusOffRecovering;
    }
    constexpr Phase phase() const { return phase_; }
    constexpr proto::CanState controller_state() const { return last_state_; }
    constexpr const Counters& counters() const { return counters_; }
    constexpr std::uint8_t attempt() const { return attempt_; }
    constexpr std::uint32_t current_backoff_ms() const { return backoff_ms(attempt_); }
    constexpr const BusOffPolicy& policy() const { return policy_; }

    static constexpr const char* to_string(Phase p)
    {
        switch (p) {
        case Phase::kActive: return "active";
        case Phase::kPassive: return "passive";
        case Phase::kBusOffBackoff: return "bus_off_backoff";
        case Phase::kBusOffRecovering: return "bus_off_recovering";
        case Phase::kFailed: return "failed";
        }
        return "unknown";
    }

private:
    constexpr std::uint32_t backoff_ms(std::uint8_t attempt) const
    {
        std::uint32_t ms = policy_.initial_backoff_ms;
        for (std::uint8_t i = 0; i < attempt && ms < policy_.max_backoff_ms; ++i) {
            ms *= 2U;
        }
        return ms > policy_.max_backoff_ms ? policy_.max_backoff_ms : ms;
    }

    constexpr void begin_backoff(std::int64_t now_ms)
    {
        phase_ = Phase::kBusOffBackoff;
        recover_at_ms_ = now_ms + backoff_ms(attempt_);
    }

    constexpr void recovery_failed(std::int64_t now_ms)
    {
        ++counters_.recovery_failures;
        if (attempt_ + 1U >= policy_.max_attempts) {
            phase_ = Phase::kFailed;
            return;
        }
        ++attempt_;
        begin_backoff(now_ms);
    }

    BusOffPolicy policy_;
    Phase phase_ = Phase::kActive;
    proto::CanState last_state_ = proto::CanState::kErrorActive;
    std::uint8_t attempt_ = 0;
    std::int64_t recover_at_ms_ = 0;
    std::int64_t recovery_deadline_ms_ = 0;
    std::int64_t active_since_ms_ = -1;
    Counters counters_{};
};

}  // namespace ecu

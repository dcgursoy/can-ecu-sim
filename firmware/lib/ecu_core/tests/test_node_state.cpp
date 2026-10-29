#include <gtest/gtest.h>

#include "ecu/node_state.hpp"

namespace {

using ecu::NodeInputs;
using ecu::NodePolicy;
using ecu::NodeStateMachine;
using proto::NodeState;

constexpr NodePolicy kController{true, 500};
constexpr NodePolicy kSensor{false, 500};

NodeInputs healthy()
{
    NodeInputs in;
    in.critical_inputs_alive = true;
    return in;
}

NodeInputs lost()
{
    NodeInputs in;
    in.critical_inputs_alive = false;
    in.critical_inputs_lost = true;
    return in;
}

NodeInputs waiting()  // after start(), before the first frame or the first timeout
{
    return NodeInputs{};
}

TEST(NodeStateMachine, StaysInBootUntilInputsArriveOrTimeOut)
{
    NodeStateMachine sm(kController);
    EXPECT_EQ(sm.state(), NodeState::kBoot);
    EXPECT_FALSE(sm.update(healthy(), 0)) << "nothing happens before start()";
    EXPECT_EQ(sm.state(), NodeState::kBoot);

    sm.start(0);
    EXPECT_EQ(sm.state(), NodeState::kBoot);
    EXPECT_FALSE(sm.update(waiting(), 5));
    EXPECT_FALSE(sm.update(waiting(), 30));
    EXPECT_EQ(sm.state(), NodeState::kBoot) << "no peers yet is not a fault";
    EXPECT_TRUE(sm.update(healthy(), 40));
    EXPECT_EQ(sm.state(), NodeState::kRunning);
    EXPECT_STREQ(sm.last_reason(), "inputs established");
    EXPECT_EQ(sm.safe_state_entries(), 0U);
}

TEST(NodeStateMachine, PeerThatNeverShowsUpMeansSafeStateAfterTheTimeout)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    EXPECT_FALSE(sm.update(waiting(), 300));
    EXPECT_TRUE(sm.update(lost(), 351));
    EXPECT_EQ(sm.state(), NodeState::kSafeState);
    EXPECT_STREQ(sm.last_reason(), "critical input never arrived");
    EXPECT_EQ(sm.safe_state_entries(), 1U);

    NodeStateMachine sensor(kSensor);
    sensor.start(0);
    EXPECT_TRUE(sensor.update(lost(), 351));
    EXPECT_EQ(sensor.state(), NodeState::kDegraded);
}

TEST(NodeStateMachine, BootWithDegradedInputsGoesStraightToDegraded)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    NodeInputs in = healthy();
    in.degraded_quality = true;
    EXPECT_TRUE(sm.update(in, 10));
    EXPECT_EQ(sm.state(), NodeState::kDegraded);
}

TEST(NodeStateMachine, ControllerEntersSafeStateOnInputLossAndRecoversAfterHold)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    sm.update(healthy(), 10);
    EXPECT_EQ(sm.state(), NodeState::kRunning);

    EXPECT_TRUE(sm.update(lost(), 1000));
    EXPECT_EQ(sm.state(), NodeState::kSafeState);
    EXPECT_STREQ(sm.last_reason(), "critical input lost");
    EXPECT_EQ(sm.entered_at_ms(), 1000);
    EXPECT_EQ(sm.safe_state_entries(), 1U);
    EXPECT_TRUE(sm.safe());

    // Inputs come back at t=2000: hold for 500 ms before leaving the safe state.
    EXPECT_FALSE(sm.update(healthy(), 2000));
    EXPECT_FALSE(sm.update(healthy(), 2499));
    EXPECT_EQ(sm.state(), NodeState::kSafeState);
    EXPECT_TRUE(sm.update(healthy(), 2500));
    EXPECT_EQ(sm.state(), NodeState::kRunning);
    EXPECT_STREQ(sm.last_reason(), "inputs healthy again");
    EXPECT_FALSE(sm.safe());

    // A glitch during the hold restarts it.
    sm.update(lost(), 3000);
    sm.update(healthy(), 3100);
    sm.update(lost(), 3300);
    sm.update(healthy(), 3400);
    EXPECT_FALSE(sm.update(healthy(), 3850));  // only 450 ms since 3400
    EXPECT_TRUE(sm.update(healthy(), 3900));
    EXPECT_EQ(sm.safe_state_entries(), 2U) << "a loss while already safe is not a new entry";
}

TEST(NodeStateMachine, DegradedQualityAndSafeStateExitIntoDegraded)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    sm.update(healthy(), 1);
    NodeInputs degraded = healthy();
    degraded.degraded_quality = true;
    EXPECT_TRUE(sm.update(degraded, 10));
    EXPECT_EQ(sm.state(), NodeState::kDegraded);
    EXPECT_TRUE(sm.update(healthy(), 20));
    EXPECT_EQ(sm.state(), NodeState::kRunning);

    sm.update(lost(), 100);
    EXPECT_EQ(sm.state(), NodeState::kSafeState);
    sm.update(degraded, 200);
    EXPECT_TRUE(sm.update(degraded, 700));
    EXPECT_EQ(sm.state(), NodeState::kDegraded);
}

TEST(NodeStateMachine, BusOffIsAnInputLoss)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    sm.update(healthy(), 1);
    NodeInputs in = healthy();
    in.bus_off = true;
    EXPECT_TRUE(sm.update(in, 5));
    EXPECT_EQ(sm.state(), NodeState::kSafeState);
    EXPECT_STREQ(sm.last_reason(), "bus off");

    NodeStateMachine booting(kController);
    booting.start(0);
    EXPECT_TRUE(booting.update(in, 5)) << "bus-off during boot is not 'waiting for peers'";
    EXPECT_EQ(booting.state(), NodeState::kSafeState);
}

TEST(NodeStateMachine, FaultIsLatchedUntilReset)
{
    NodeStateMachine sm(kController);
    sm.start(0);
    sm.update(healthy(), 1);
    NodeInputs in = healthy();
    in.can_failed = true;
    EXPECT_TRUE(sm.update(in, 10));
    EXPECT_EQ(sm.state(), NodeState::kFault);
    EXPECT_TRUE(sm.safe());
    EXPECT_FALSE(sm.update(healthy(), 20)) << "healthy inputs do not clear a fault";
    EXPECT_EQ(sm.state(), NodeState::kFault);
    sm.reset(30);
    EXPECT_EQ(sm.state(), NodeState::kSafeState) << "a reset is not evidence that the inputs are healthy";
    EXPECT_STREQ(sm.last_reason(), "fault reset");
    EXPECT_EQ(sm.safe_state_entries(), 0U) << "the reset path is not counted as an entry";
    EXPECT_FALSE(sm.update(healthy(), 100));  // hold starts at the first healthy update
    EXPECT_FALSE(sm.update(healthy(), 599));
    EXPECT_TRUE(sm.update(healthy(), 600)) << "leaves through the normal 500 ms hold";
    EXPECT_EQ(sm.state(), NodeState::kRunning);
    EXPECT_EQ(sm.transitions(), 4U);  // boot->running, running->fault, fault->safe, safe->running

    NodeStateMachine sensor(kSensor);
    sensor.start(0);
    sensor.update(healthy(), 1);
    sensor.update(in, 10);
    EXPECT_EQ(sensor.state(), NodeState::kFault);
    sensor.reset(20);
    EXPECT_EQ(sensor.state(), NodeState::kDegraded);
    EXPECT_TRUE(sensor.update(healthy(), 30));
    EXPECT_EQ(sensor.state(), NodeState::kRunning);
}

TEST(NodeStateMachine, ResetOutsideFaultIsIgnoredAndThePolicyCanComeFromRuntimeValues)
{
    volatile std::uint32_t hold = 250;
    const NodePolicy policy{true, hold};
    NodeStateMachine sm(policy);
    sm.start(0);
    sm.update(healthy(), 1);
    sm.reset(2);  // not in FAULT: nothing happens, nothing is counted
    EXPECT_EQ(sm.state(), NodeState::kRunning);
    EXPECT_EQ(sm.transitions(), 1U);
    sm.update(lost(), 10);
    sm.update(healthy(), 20);
    EXPECT_FALSE(sm.update(healthy(), 269));
    EXPECT_TRUE(sm.update(healthy(), 270)) << "the hold time is the policy's: 250 ms here";
    EXPECT_EQ(sm.state(), NodeState::kRunning);
}

TEST(NodeStateMachine, SensorPolicyDegradesInsteadOfSafeState)
{
    NodeStateMachine sm(kSensor);
    sm.start(0);
    sm.update(healthy(), 1);
    EXPECT_TRUE(sm.update(lost(), 10));
    EXPECT_EQ(sm.state(), NodeState::kDegraded);
    EXPECT_EQ(sm.safe_state_entries(), 0U);
    EXPECT_TRUE(sm.update(healthy(), 20)) << "no hold time outside the safe state";
    EXPECT_EQ(sm.state(), NodeState::kRunning);
}

}  // namespace

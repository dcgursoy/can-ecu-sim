// Node state machine shared by both ECU roles. The states are the DBC value
// table NodeState, so the state is published verbatim in the heartbeat.
//
//   BOOT        from start() until the critical inputs have either arrived
//               (-> RUNNING) or timed out (-> SAFE_STATE / DEGRADED); waiting
//               for peers at power-up is not a fault and is not counted as one
//   RUNNING     all monitored inputs alive, no quality problems
//   DEGRADED    inputs alive but of reduced quality (CRC/counter faults above
//               the threshold, sensor reporting DEGRADED, battery warnings), or,
//               for a node without a safe state of its own, inputs lost
//   SAFE_STATE  critical inputs lost (controller only): actuators off,
//               commands forced to their safe values; left automatically after
//               the inputs have been healthy for recovery_hold_ms
//   FAULT       latched: CAN recovery exhausted; cleared only by reset()
//
// Why an automatic exit from SAFE_STATE: the loss of a periodic message is a
// transient in most real systems (a reset peer, a brief bus overload) and the
// recovery time is one of the numbers this project measures. Why a hold time:
// so one good frame cannot flip the controller between NORMAL and SAFE_STATE
// at the message rate. Why FAULT is latched: a node that lost its bus five
// times in a row has a hardware problem that a state machine cannot fix.
#pragma once

#include <cstdint>

#include "protocol/messages.hpp"

namespace ecu {

struct NodeInputs {
    bool critical_inputs_alive = false;  ///< every message this node depends on is alive
    bool critical_inputs_lost = false;   ///< at least one of them timed out (also at boot: never arrived)
    bool degraded_quality = false;       ///< alive but with faults or warnings
    bool can_failed = false;             ///< CanErrorStateMachine::failed()
    bool bus_off = false;                ///< CanErrorStateMachine::bus_off()
};

struct NodePolicy {
    bool safe_state_on_input_loss = true;  ///< false: a node that merely degrades
    std::uint32_t recovery_hold_ms = 500;
};

class NodeStateMachine {
public:
    constexpr explicit NodeStateMachine(NodePolicy policy) : policy_(policy) {}

    /// Arm the machine: it stays in BOOT until update() sees the inputs settle.
    constexpr void start(std::int64_t now_ms)
    {
        started_ = true;
        entered_at_ms_ = now_ms;
    }

    /// Evaluate the inputs; returns true when the state changed.
    constexpr bool update(const NodeInputs& in, std::int64_t now_ms)
    {
        const proto::NodeState before = state_;
        if (!started_) {
            return false;
        }
        if (in.can_failed) {
            transition(proto::NodeState::kFault, "can recovery exhausted", now_ms);
            healthy_since_ms_ = -1;
            return state_ != before;
        }
        if (state_ == proto::NodeState::kFault) {
            return false;  // latched until reset()
        }
        const bool inputs_ok = in.critical_inputs_alive && !in.bus_off;
        if (state_ == proto::NodeState::kBoot) {
            // Waiting for the peers after power-up is not a fault: leave BOOT only
            // when the inputs are there, or when the first timeout says they are not.
            if (inputs_ok) {
                transition(in.degraded_quality ? proto::NodeState::kDegraded : proto::NodeState::kRunning,
                           "inputs established", now_ms);
                healthy_since_ms_ = now_ms;
            } else if (in.critical_inputs_lost || in.bus_off) {
                const char* reason = in.bus_off ? "bus off" : "critical input never arrived";
                transition(policy_.safe_state_on_input_loss ? proto::NodeState::kSafeState : proto::NodeState::kDegraded,
                           reason, now_ms);
            }
            return state_ != before;
        }
        if (!inputs_ok) {
            healthy_since_ms_ = -1;
            const char* reason = in.bus_off ? "bus off" : "critical input lost";
            if (policy_.safe_state_on_input_loss) {
                transition(proto::NodeState::kSafeState, reason, now_ms);
            } else {
                transition(proto::NodeState::kDegraded, reason, now_ms);
            }
            return state_ != before;
        }
        if (healthy_since_ms_ < 0) {
            healthy_since_ms_ = now_ms;
        }
        if (state_ == proto::NodeState::kSafeState) {
            if (now_ms - healthy_since_ms_ < policy_.recovery_hold_ms) {
                return false;  // hold before leaving the safe state
            }
            transition(in.degraded_quality ? proto::NodeState::kDegraded : proto::NodeState::kRunning,
                       "inputs healthy again", now_ms);
            return state_ != before;
        }
        if (in.degraded_quality) {
            transition(proto::NodeState::kDegraded, "input quality degraded", now_ms);
        } else {
            transition(proto::NodeState::kRunning, "inputs nominal", now_ms);
        }
        return state_ != before;
    }

    /// Clear a latched FAULT (operator or test action). A reset is not evidence
    /// that the inputs are healthy: a node with a safe state re-enters SAFE_STATE
    /// and leaves it through the normal recovery hold; a node without one goes
    /// to DEGRADED and the next update() settles it. Not counted as a safe-state
    /// entry.
    constexpr void reset(std::int64_t now_ms)
    {
        if (state_ != proto::NodeState::kFault) {
            return;
        }
        healthy_since_ms_ = -1;
        transition(policy_.safe_state_on_input_loss ? proto::NodeState::kSafeState : proto::NodeState::kDegraded,
                   "fault reset", now_ms, false);
    }

    constexpr proto::NodeState state() const { return state_; }
    constexpr const char* last_reason() const { return last_reason_; }
    constexpr std::int64_t entered_at_ms() const { return entered_at_ms_; }
    constexpr std::uint32_t safe_state_entries() const { return safe_state_entries_; }
    constexpr std::uint32_t transitions() const { return transitions_; }
    constexpr bool safe() const { return state_ == proto::NodeState::kSafeState || state_ == proto::NodeState::kFault; }

private:
    constexpr void transition(proto::NodeState next, const char* reason, std::int64_t now_ms,
                              bool count_safe_entry = true)
    {
        if (next == state_) {
            return;
        }
        if (count_safe_entry && next == proto::NodeState::kSafeState) {
            ++safe_state_entries_;
        }
        state_ = next;
        last_reason_ = reason;
        entered_at_ms_ = now_ms;
        ++transitions_;
    }

    NodePolicy policy_;
    bool started_ = false;
    proto::NodeState state_ = proto::NodeState::kBoot;
    const char* last_reason_ = "boot";
    std::int64_t entered_at_ms_ = 0;
    std::int64_t healthy_since_ms_ = -1;
    std::uint32_t safe_state_entries_ = 0;
    std::uint32_t transitions_ = 0;
};

}  // namespace ecu

//! Node state machine shared by both ECU roles, the port of `ecu/node_state.hpp`.
//! The states are the DBC value table `NodeState`, so the state is published
//! verbatim in the heartbeat.
//!
//!   BOOT        from `start` until the critical inputs have either arrived
//!               (-> RUNNING) or timed out (-> SAFE_STATE / DEGRADED); waiting
//!               for peers at power-up is not a fault and is not counted as one
//!   RUNNING     all monitored inputs alive, no quality problems
//!   DEGRADED    inputs alive but of reduced quality, or, for a node without a
//!               safe state of its own, inputs lost
//!   SAFE_STATE  critical inputs lost (controller only); left automatically
//!               after the inputs have been healthy for `recovery_hold_ms`
//!   FAULT       latched: CAN recovery exhausted; cleared only by `reset`

use crate::messages::NodeState;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct NodeInputs {
    /// Every message this node depends on is alive.
    pub critical_inputs_alive: bool,
    /// At least one of them timed out (also at boot: never arrived).
    pub critical_inputs_lost: bool,
    /// Alive but with faults or warnings.
    pub degraded_quality: bool,
    /// `CanErrorStateMachine::failed()`
    pub can_failed: bool,
    /// `CanErrorStateMachine::bus_off()`
    pub bus_off: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NodePolicy {
    /// false: a node that merely degrades.
    pub safe_state_on_input_loss: bool,
    pub recovery_hold_ms: u32,
}

#[derive(Clone, Copy, Debug)]
pub struct NodeStateMachine {
    policy: NodePolicy,
    started: bool,
    state: NodeState,
    last_reason: &'static str,
    entered_at_ms: i64,
    healthy_since_ms: i64,
    safe_state_entries: u32,
    transitions: u32,
}

impl NodeStateMachine {
    pub const fn new(policy: NodePolicy) -> Self {
        Self {
            policy,
            started: false,
            state: NodeState::Boot,
            last_reason: "boot",
            entered_at_ms: 0,
            healthy_since_ms: -1,
            safe_state_entries: 0,
            transitions: 0,
        }
    }

    /// Arm the machine: it stays in BOOT until `update` sees the inputs settle.
    pub fn start(&mut self, now_ms: i64) {
        self.started = true;
        self.entered_at_ms = now_ms;
    }

    /// Evaluate the inputs; returns true when the state changed.
    pub fn update(&mut self, input: &NodeInputs, now_ms: i64) -> bool {
        let before = self.state;
        if !self.started {
            return false;
        }
        if input.can_failed {
            self.transition(NodeState::Fault, "can recovery exhausted", now_ms, true);
            self.healthy_since_ms = -1;
            return self.state != before;
        }
        if self.state == NodeState::Fault {
            return false; // latched until reset()
        }
        let inputs_ok = input.critical_inputs_alive && !input.bus_off;
        let loss_state = if self.policy.safe_state_on_input_loss {
            NodeState::SafeState
        } else {
            NodeState::Degraded
        };
        if self.state == NodeState::Boot {
            // Waiting for the peers after power-up is not a fault: leave BOOT only
            // when the inputs are there, or when the first timeout says they are not.
            if inputs_ok {
                let next = if input.degraded_quality {
                    NodeState::Degraded
                } else {
                    NodeState::Running
                };
                self.transition(next, "inputs established", now_ms, true);
                self.healthy_since_ms = now_ms;
            } else if input.critical_inputs_lost || input.bus_off {
                let reason = if input.bus_off {
                    "bus off"
                } else {
                    "critical input never arrived"
                };
                self.transition(loss_state, reason, now_ms, true);
            }
            return self.state != before;
        }
        if !inputs_ok {
            self.healthy_since_ms = -1;
            let reason = if input.bus_off {
                "bus off"
            } else {
                "critical input lost"
            };
            self.transition(loss_state, reason, now_ms, true);
            return self.state != before;
        }
        if self.healthy_since_ms < 0 {
            self.healthy_since_ms = now_ms;
        }
        if self.state == NodeState::SafeState {
            if now_ms - self.healthy_since_ms < i64::from(self.policy.recovery_hold_ms) {
                return false; // hold before leaving the safe state
            }
            let next = if input.degraded_quality {
                NodeState::Degraded
            } else {
                NodeState::Running
            };
            self.transition(next, "inputs healthy again", now_ms, true);
            return self.state != before;
        }
        if input.degraded_quality {
            self.transition(NodeState::Degraded, "input quality degraded", now_ms, true);
        } else {
            self.transition(NodeState::Running, "inputs nominal", now_ms, true);
        }
        self.state != before
    }

    /// Clear a latched FAULT (operator or test action). A reset is not evidence
    /// that the inputs are healthy: a node with a safe state re-enters
    /// SAFE_STATE and leaves it through the normal recovery hold; a node
    /// without one goes to DEGRADED and the next `update` settles it. Not
    /// counted as a safe-state entry.
    pub fn reset(&mut self, now_ms: i64) {
        if self.state != NodeState::Fault {
            return;
        }
        self.healthy_since_ms = -1;
        let next = if self.policy.safe_state_on_input_loss {
            NodeState::SafeState
        } else {
            NodeState::Degraded
        };
        self.transition(next, "fault reset", now_ms, false);
    }

    pub const fn state(&self) -> NodeState {
        self.state
    }

    pub const fn last_reason(&self) -> &'static str {
        self.last_reason
    }

    pub const fn entered_at_ms(&self) -> i64 {
        self.entered_at_ms
    }

    pub const fn safe_state_entries(&self) -> u32 {
        self.safe_state_entries
    }

    pub const fn transitions(&self) -> u32 {
        self.transitions
    }

    pub const fn safe(&self) -> bool {
        matches!(self.state, NodeState::SafeState | NodeState::Fault)
    }

    fn transition(
        &mut self,
        next: NodeState,
        reason: &'static str,
        now_ms: i64,
        count_safe_entry: bool,
    ) {
        if next == self.state {
            return;
        }
        if count_safe_entry && next == NodeState::SafeState {
            self.safe_state_entries = self.safe_state_entries.saturating_add(1);
        }
        self.state = next;
        self.last_reason = reason;
        self.entered_at_ms = now_ms;
        self.transitions = self.transitions.saturating_add(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CONTROLLER: NodePolicy = NodePolicy {
        safe_state_on_input_loss: true,
        recovery_hold_ms: 500,
    };
    const SENSOR: NodePolicy = NodePolicy {
        safe_state_on_input_loss: false,
        recovery_hold_ms: 500,
    };

    fn healthy() -> NodeInputs {
        NodeInputs {
            critical_inputs_alive: true,
            ..NodeInputs::default()
        }
    }

    fn lost() -> NodeInputs {
        NodeInputs {
            critical_inputs_lost: true,
            ..NodeInputs::default()
        }
    }

    #[test]
    fn boot_waits_for_the_inputs_then_runs() {
        let mut sm = NodeStateMachine::new(CONTROLLER);
        assert!(!sm.update(&healthy(), 0), "nothing happens before start()");
        sm.start(0);
        assert!(!sm.update(&NodeInputs::default(), 30));
        assert_eq!(sm.state(), NodeState::Boot, "no peers yet is not a fault");
        assert!(sm.update(&healthy(), 40));
        assert_eq!(sm.state(), NodeState::Running);
        assert_eq!(sm.last_reason(), "inputs established");
        assert_eq!(sm.safe_state_entries(), 0);
    }

    #[test]
    fn a_peer_that_never_shows_up_means_safe_state_or_degraded() {
        let mut sm = NodeStateMachine::new(CONTROLLER);
        sm.start(0);
        assert!(sm.update(&lost(), 351));
        assert_eq!(sm.state(), NodeState::SafeState);
        assert_eq!(sm.last_reason(), "critical input never arrived");
        assert_eq!(sm.safe_state_entries(), 1);
        let mut sensor = NodeStateMachine::new(SENSOR);
        sensor.start(0);
        assert!(sensor.update(&lost(), 351));
        assert_eq!(sensor.state(), NodeState::Degraded);
        assert_eq!(sensor.safe_state_entries(), 0);
    }

    #[test]
    fn the_controller_enters_safe_state_and_recovers_after_the_hold() {
        let mut sm = NodeStateMachine::new(CONTROLLER);
        sm.start(0);
        sm.update(&healthy(), 10);
        assert!(sm.update(&lost(), 1000));
        assert_eq!(sm.state(), NodeState::SafeState);
        assert!(sm.safe());
        assert!(!sm.update(&healthy(), 2000));
        assert!(!sm.update(&healthy(), 2499));
        assert!(sm.update(&healthy(), 2500));
        assert_eq!(sm.state(), NodeState::Running);
        assert_eq!(sm.last_reason(), "inputs healthy again");
        // A glitch during the hold restarts it.
        sm.update(&lost(), 3000);
        sm.update(&healthy(), 3100);
        sm.update(&lost(), 3300);
        sm.update(&healthy(), 3400);
        assert!(!sm.update(&healthy(), 3850));
        assert!(sm.update(&healthy(), 3900));
        assert_eq!(
            sm.safe_state_entries(),
            2,
            "a loss while already safe is not a new entry"
        );
    }

    #[test]
    fn degraded_quality_bus_off_and_the_latched_fault() {
        let mut sm = NodeStateMachine::new(CONTROLLER);
        sm.start(0);
        sm.update(&healthy(), 1);
        let degraded = NodeInputs {
            degraded_quality: true,
            ..healthy()
        };
        assert!(sm.update(&degraded, 10));
        assert_eq!(sm.state(), NodeState::Degraded);
        assert!(sm.update(&healthy(), 20));
        assert_eq!(sm.state(), NodeState::Running);
        let bus_off = NodeInputs {
            bus_off: true,
            ..healthy()
        };
        assert!(sm.update(&bus_off, 30));
        assert_eq!(sm.state(), NodeState::SafeState);
        assert_eq!(sm.last_reason(), "bus off");
        let failed = NodeInputs {
            can_failed: true,
            ..healthy()
        };
        assert!(sm.update(&failed, 40));
        assert_eq!(sm.state(), NodeState::Fault);
        assert!(
            !sm.update(&healthy(), 50),
            "healthy inputs do not clear a fault"
        );
        sm.reset(60);
        assert_eq!(
            sm.state(),
            NodeState::SafeState,
            "a reset is not evidence that the inputs are healthy"
        );
        assert_eq!(sm.last_reason(), "fault reset");
        assert_eq!(sm.safe_state_entries(), 1, "the reset path is not counted");
        assert!(!sm.update(&healthy(), 100));
        assert!(sm.update(&healthy(), 600), "leaves through the normal hold");
        assert_eq!(sm.state(), NodeState::Running);
        assert_eq!(sm.transitions(), 7);
        let mut sensor = NodeStateMachine::new(SENSOR);
        sensor.start(0);
        sensor.update(&healthy(), 1);
        sensor.update(&failed, 10);
        sensor.reset(20);
        assert_eq!(sensor.state(), NodeState::Degraded);
        assert!(sensor.update(&healthy(), 30));
        assert_eq!(sensor.state(), NodeState::Running);
        sensor.reset(40);
        assert_eq!(
            sensor.state(),
            NodeState::Running,
            "reset outside FAULT is ignored"
        );
    }
}

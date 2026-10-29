//! CAN error handling policy on top of the controller states the driver
//! reports, the port of `ecu/can_error_sm.hpp`:
//!
//!   error-active      transmit normally
//!   error-passive     keep transmitting, count the event
//!   bus-off           stop transmitting; after a backoff, ask the driver to
//!                     recover; double the backoff on every attempt up to a cap;
//!                     after `max_attempts` consecutive failures latch "failed"
//!                     and let the node state machine enter FAULT
//!   recovered         when the controller is error-active again; attempts are
//!                     forgotten once the controller has stayed error-active
//!                     for `stable_ms` without interruption
//!
//! Two input paths feed the machine: `on_controller_state` is edge-triggered
//! (a state-change callback; a driver like bxCAN raises it only when a worse
//! state is entered) and `poll_controller_state` is level-triggered (the
//! supervisor reads the controller state every tick), reacting only to the
//! changes the callback path cannot deliver.

use crate::messages::CanState;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct BusOffPolicy {
    pub initial_backoff_ms: u32,
    pub max_backoff_ms: u32,
    /// Time allowed for the controller to report error-active.
    pub recovery_timeout_ms: u32,
    pub max_attempts: u8,
    /// Error-active this long forgets past attempts.
    pub stable_ms: u32,
}

impl Default for BusOffPolicy {
    fn default() -> Self {
        Self {
            initial_backoff_ms: 100,
            max_backoff_ms: 1600,
            recovery_timeout_ms: 500,
            max_attempts: 5,
            stable_ms: 10000,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Phase {
    Active,
    Passive,
    /// Waiting for the backoff to elapse.
    BusOffBackoff,
    /// Recovery requested, waiting for error-active.
    BusOffRecovering,
    /// Attempts exhausted, latched until `reset`.
    Failed,
}

impl Phase {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Active => "active",
            Self::Passive => "passive",
            Self::BusOffBackoff => "bus_off_backoff",
            Self::BusOffRecovering => "bus_off_recovering",
            Self::Failed => "failed",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    None,
    StartRecovery,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Counters {
    pub bus_off_events: u32,
    pub passive_events: u32,
    pub recovery_attempts: u32,
    pub recoveries_ok: u32,
    pub recovery_failures: u32,
    pub last_bus_off_ms: i64,
    pub last_recovery_ms: i64,
    /// Bus-off to error-active.
    pub last_recovery_duration_ms: i64,
}

#[derive(Clone, Copy, Debug)]
pub struct CanErrorStateMachine {
    policy: BusOffPolicy,
    phase: Phase,
    last_state: CanState,
    attempt: u8,
    recover_at_ms: i64,
    recovery_deadline_ms: i64,
    active_since_ms: i64,
    counters: Counters,
}

impl CanErrorStateMachine {
    pub const fn new(policy: BusOffPolicy) -> Self {
        Self {
            policy,
            phase: Phase::Active,
            last_state: CanState::ErrorActive,
            attempt: 0,
            recover_at_ms: 0,
            recovery_deadline_ms: 0,
            active_since_ms: -1,
            counters: Counters {
                bus_off_events: 0,
                passive_events: 0,
                recovery_attempts: 0,
                recoveries_ok: 0,
                recovery_failures: 0,
                last_bus_off_ms: -1,
                last_recovery_ms: -1,
                last_recovery_duration_ms: -1,
            },
        }
    }

    /// Edge-triggered input: the driver (or simulated controller) reported a new state.
    pub fn on_controller_state(&mut self, state: CanState, now_ms: i64) {
        self.last_state = state;
        if self.phase == Phase::Failed {
            return; // latched; a reset() clears it
        }
        match state {
            CanState::BusOff => {
                if self.phase == Phase::BusOffBackoff {
                    return; // duplicate report while already waiting
                }
                if self.phase == Phase::BusOffRecovering {
                    self.recovery_failed(now_ms); // went off again before becoming active
                    return;
                }
                self.counters.bus_off_events = self.counters.bus_off_events.saturating_add(1);
                self.counters.last_bus_off_ms = now_ms;
                if self.counters.last_recovery_ms >= 0
                    && now_ms - self.counters.last_recovery_ms < i64::from(self.policy.stable_ms)
                {
                    // The previous recovery did not hold: a failed attempt, so the
                    // backoff doubles and a flapping node eventually latches FAULT.
                    self.recovery_failed(now_ms);
                } else {
                    self.begin_backoff(now_ms);
                }
            }
            CanState::ErrorPassive => {
                if self.phase == Phase::Active {
                    self.counters.passive_events = self.counters.passive_events.saturating_add(1);
                    self.phase = Phase::Passive;
                }
            }
            CanState::ErrorActive => {
                if self.phase == Phase::BusOffRecovering {
                    self.counters.recoveries_ok = self.counters.recoveries_ok.saturating_add(1);
                    self.counters.last_recovery_ms = now_ms;
                    self.counters.last_recovery_duration_ms =
                        now_ms - self.counters.last_bus_off_ms;
                }
                // Idempotent: a repeated error-active report must not restart the stable window.
                if matches!(self.phase, Phase::Passive | Phase::BusOffRecovering) {
                    self.phase = Phase::Active;
                    self.active_since_ms = now_ms;
                }
            }
            CanState::Stopped => {}
        }
    }

    /// Level-triggered input: the controller state as read right now. Reacts
    /// only to the improvements a status-change interrupt never signals and
    /// to a bus-off that arrived without a callback; a bus-off reading during
    /// backoff or recovery is the condition already being handled.
    pub fn poll_controller_state(&mut self, state: CanState, now_ms: i64) {
        if state == self.last_state || state == CanState::Stopped {
            return;
        }
        if state == CanState::BusOff
            && matches!(
                self.phase,
                Phase::BusOffBackoff | Phase::BusOffRecovering | Phase::Failed
            )
        {
            return;
        }
        self.on_controller_state(state, now_ms);
    }

    /// Periodic call; returns `StartRecovery` when the HAL should recover.
    pub fn tick(&mut self, now_ms: i64) -> Action {
        match self.phase {
            Phase::BusOffBackoff => {
                if now_ms >= self.recover_at_ms {
                    self.phase = Phase::BusOffRecovering;
                    self.recovery_deadline_ms = now_ms + i64::from(self.policy.recovery_timeout_ms);
                    self.counters.recovery_attempts =
                        self.counters.recovery_attempts.saturating_add(1);
                    return Action::StartRecovery;
                }
            }
            Phase::BusOffRecovering => {
                if now_ms > self.recovery_deadline_ms {
                    self.recovery_failed(now_ms);
                }
            }
            Phase::Active => {
                if self.attempt != 0
                    && self.active_since_ms >= 0
                    && now_ms - self.active_since_ms >= i64::from(self.policy.stable_ms)
                {
                    self.attempt = 0; // healthy long enough to forget the history
                }
            }
            Phase::Passive | Phase::Failed => {}
        }
        Action::None
    }

    /// Result of the HAL's recover() call (false: the driver refused).
    pub fn on_recovery_request_result(&mut self, accepted: bool, now_ms: i64) {
        if !accepted && self.phase == Phase::BusOffRecovering {
            self.recovery_failed(now_ms);
        }
    }

    /// Clear a latched failure (test command or operator action). If the
    /// controller is still bus-off the backoff/recovery cycle starts again
    /// from the first attempt.
    pub fn reset(&mut self, now_ms: i64) {
        self.attempt = 0;
        if self.phase != Phase::Failed {
            return;
        }
        if self.last_state == CanState::BusOff {
            self.begin_backoff(now_ms);
        } else {
            self.phase = if self.last_state == CanState::ErrorPassive {
                Phase::Passive
            } else {
                Phase::Active
            };
            self.active_since_ms = now_ms;
        }
    }

    pub const fn tx_allowed(&self) -> bool {
        matches!(self.phase, Phase::Active | Phase::Passive)
    }

    pub const fn failed(&self) -> bool {
        matches!(self.phase, Phase::Failed)
    }

    pub const fn bus_off(&self) -> bool {
        matches!(self.phase, Phase::BusOffBackoff | Phase::BusOffRecovering)
    }

    pub const fn phase(&self) -> Phase {
        self.phase
    }

    pub const fn controller_state(&self) -> CanState {
        self.last_state
    }

    pub const fn counters(&self) -> &Counters {
        &self.counters
    }

    pub const fn attempt(&self) -> u8 {
        self.attempt
    }

    pub const fn current_backoff_ms(&self) -> u32 {
        self.backoff_ms(self.attempt)
    }

    pub const fn policy(&self) -> &BusOffPolicy {
        &self.policy
    }

    const fn backoff_ms(&self, attempt: u8) -> u32 {
        let mut ms = self.policy.initial_backoff_ms;
        let mut i = 0;
        while i < attempt && ms < self.policy.max_backoff_ms {
            ms *= 2;
            i += 1;
        }
        if ms > self.policy.max_backoff_ms {
            self.policy.max_backoff_ms
        } else {
            ms
        }
    }

    fn begin_backoff(&mut self, now_ms: i64) {
        self.phase = Phase::BusOffBackoff;
        self.recover_at_ms = now_ms + i64::from(self.backoff_ms(self.attempt));
    }

    fn recovery_failed(&mut self, now_ms: i64) {
        self.counters.recovery_failures = self.counters.recovery_failures.saturating_add(1);
        if u32::from(self.attempt) + 1 >= u32::from(self.policy.max_attempts) {
            self.phase = Phase::Failed;
            return;
        }
        self.attempt = self.attempt.saturating_add(1);
        self.begin_backoff(now_ms);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const POLICY: BusOffPolicy = BusOffPolicy {
        initial_backoff_ms: 100,
        max_backoff_ms: 1600,
        recovery_timeout_ms: 500,
        max_attempts: 5,
        stable_ms: 10000,
    };

    #[test]
    fn passive_is_counted_and_still_transmits() {
        let mut sm = CanErrorStateMachine::new(POLICY);
        assert!(sm.tx_allowed());
        sm.on_controller_state(CanState::ErrorPassive, 1000);
        assert_eq!(sm.phase(), Phase::Passive);
        assert!(sm.tx_allowed());
        sm.on_controller_state(CanState::ErrorPassive, 1001);
        assert_eq!(sm.counters().passive_events, 1, "no double count");
        assert_eq!(sm.tick(1002), Action::None);
        sm.on_controller_state(CanState::ErrorActive, 1003);
        assert_eq!(sm.phase(), Phase::Active);
    }

    #[test]
    fn bus_off_backs_off_then_recovers() {
        let mut sm = CanErrorStateMachine::new(POLICY);
        sm.on_controller_state(CanState::BusOff, 1000);
        assert_eq!(sm.phase(), Phase::BusOffBackoff);
        assert!(!sm.tx_allowed() && sm.bus_off());
        assert_eq!(sm.current_backoff_ms(), 100);
        assert_eq!(sm.tick(1099), Action::None);
        assert_eq!(sm.tick(1100), Action::StartRecovery);
        assert_eq!(sm.counters().recovery_attempts, 1);
        sm.on_recovery_request_result(true, 1100);
        assert_eq!(
            sm.tick(1200),
            Action::None,
            "no second request while recovering"
        );
        sm.on_controller_state(CanState::ErrorActive, 1103);
        assert_eq!(sm.phase(), Phase::Active);
        assert_eq!(sm.counters().recoveries_ok, 1);
        assert_eq!(sm.counters().last_recovery_duration_ms, 103);
        sm.on_controller_state(CanState::BusOff, 1103 + 10000);
        assert_eq!(
            sm.current_backoff_ms(),
            100,
            "a bus-off after a recovery that held is a fresh event"
        );
        assert_eq!(sm.counters().recovery_failures, 0);
    }

    #[test]
    fn a_recovery_that_does_not_hold_doubles_the_backoff_until_fault() {
        let mut sm = CanErrorStateMachine::new(POLICY);
        let mut t = 0;
        for (cycle, expected) in [100u32, 200, 400, 800, 1600].into_iter().enumerate() {
            sm.on_controller_state(CanState::BusOff, t);
            assert!(!sm.failed(), "cycle {cycle}");
            assert_eq!(sm.current_backoff_ms(), expected);
            t += i64::from(expected);
            assert_eq!(sm.tick(t), Action::StartRecovery);
            t += 3;
            sm.on_controller_state(CanState::ErrorActive, t);
            assert_eq!(sm.phase(), Phase::Active);
            t += 7;
        }
        assert_eq!(sm.counters().recoveries_ok, 5);
        assert_eq!(sm.counters().recovery_failures, 4);
        sm.on_controller_state(CanState::BusOff, t);
        assert!(sm.failed());
        assert_eq!(sm.counters().recovery_failures, 5);
        assert_eq!(sm.counters().bus_off_events, 6);
        sm.on_controller_state(CanState::ErrorActive, t + 1); // ignored while failed, but remembered
        sm.poll_controller_state(CanState::BusOff, t + 2);
        assert!(sm.failed());
        sm.reset(t + 3);
        assert_eq!(sm.phase(), Phase::Active);
        assert_eq!(sm.attempt(), 0);
    }

    #[test]
    fn timeouts_refusals_polling_and_reset_while_still_bus_off() {
        let mut sm = CanErrorStateMachine::new(POLICY);
        sm.on_controller_state(CanState::BusOff, 0);
        assert_eq!(sm.tick(100), Action::StartRecovery);
        sm.on_recovery_request_result(false, 100);
        assert_eq!(
            (sm.phase(), sm.attempt(), sm.counters().recovery_failures),
            (Phase::BusOffBackoff, 1, 1)
        );
        assert_eq!(sm.current_backoff_ms(), 200);
        assert_eq!(sm.tick(300), Action::StartRecovery);
        sm.poll_controller_state(CanState::BusOff, 302); // sequence still running: not a failure
        assert_eq!(sm.phase(), Phase::BusOffRecovering);
        sm.tick(801); // recovery timeout: attempt 2
        assert_eq!(sm.attempt(), 2);
        // Exhaust: attempts 3, 4 fail, the fifth too.
        let mut t = 801;
        while !sm.failed() {
            t += 100_000;
            sm.tick(t);
            t += 100_000;
            sm.tick(t);
        }
        assert_eq!(sm.controller_state(), CanState::BusOff);
        sm.reset(t);
        assert_eq!(
            sm.phase(),
            Phase::BusOffBackoff,
            "controller still bus-off: back to the first backoff"
        );
        assert_eq!(sm.current_backoff_ms(), 100);
        assert_eq!(sm.tick(t + 100), Action::StartRecovery);
        sm.poll_controller_state(CanState::ErrorActive, t + 104);
        assert_eq!(sm.phase(), Phase::Active);
        assert_eq!(Phase::BusOffRecovering.as_str(), "bus_off_recovering");
        assert_eq!(sm.policy().max_attempts, 5);
    }

    #[test]
    fn the_attempt_count_is_forgotten_after_the_stable_period() {
        let mut sm = CanErrorStateMachine::new(POLICY);
        sm.on_controller_state(CanState::BusOff, 0);
        sm.tick(100);
        sm.tick(601);
        sm.tick(801);
        sm.on_controller_state(CanState::ErrorActive, 805);
        assert_eq!(sm.attempt(), 1);
        sm.on_controller_state(CanState::ErrorActive, 5000); // the HAL folds error-warning into error-active
        sm.tick(805 + 9999);
        assert_eq!(
            sm.attempt(),
            1,
            "not stable yet; the window counts from the first error-active"
        );
        sm.tick(805 + 10000);
        assert_eq!(sm.attempt(), 0);
        let capped = CanErrorStateMachine::new(BusOffPolicy {
            max_backoff_ms: 500,
            ..POLICY
        });
        assert_eq!(capped.backoff_ms(4), 500);
    }
}

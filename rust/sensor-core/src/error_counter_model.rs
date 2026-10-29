//! Model of a CAN controller's fault-confinement counters (ISO 11898-1), the
//! port of `ecu/error_counter_model.hpp`, used by the simulated controller
//! behind the test hooks to turn injected error events into the states a real
//! peripheral would report. A virtual bus never produces electrical errors, so
//! this is how error-passive and bus-off are exercised without hardware.
//!
//! Rules implemented (the common cases):
//!   transmit error      TEC += 8
//!   transmit success    TEC -= 1 (if > 0)
//!   receive error       REC += 1 (REC above 127 is not increased further)
//!   receive success     REC -= 1, or REC := 127 if it was above 127
//!   state               bus-off if TEC > 255, error-passive if TEC > 127 or
//!                       REC > 127, else error-active (warning at >= 96)
//!   bus-off recovery    TEC := 0, REC := 0 after 128 occurrences of 11
//!                       recessive bits; the adapter models that delay

use crate::messages::CanState;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Event {
    TxError,
    TxSuccess,
    RxError,
    RxSuccess,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct ErrorCounterModel {
    tec: u16,
    rec: u16,
    bus_off: bool,
}

impl ErrorCounterModel {
    pub const WARNING_LIMIT: u16 = 96;
    pub const PASSIVE_LIMIT: u16 = 127;
    pub const BUS_OFF_LIMIT: u16 = 255;

    pub const fn new() -> Self {
        Self {
            tec: 0,
            rec: 0,
            bus_off: false,
        }
    }

    pub fn apply(&mut self, event: Event) {
        if self.bus_off {
            return; // counters are frozen until the recovery sequence completes
        }
        match event {
            Event::TxError => {
                self.tec += 8;
                if self.tec > Self::BUS_OFF_LIMIT {
                    self.bus_off = true;
                }
            }
            Event::TxSuccess => self.tec = self.tec.saturating_sub(1),
            Event::RxError => {
                if self.rec <= Self::PASSIVE_LIMIT {
                    self.rec += 1;
                }
            }
            Event::RxSuccess => {
                if self.rec > Self::PASSIVE_LIMIT {
                    self.rec = Self::PASSIVE_LIMIT;
                } else {
                    self.rec = self.rec.saturating_sub(1);
                }
            }
        }
    }

    /// Completion of the bus-off recovery sequence.
    pub fn recover(&mut self) {
        self.tec = 0;
        self.rec = 0;
        self.bus_off = false;
    }

    pub const fn state(&self) -> CanState {
        if self.bus_off {
            CanState::BusOff
        } else if self.tec > Self::PASSIVE_LIMIT || self.rec > Self::PASSIVE_LIMIT {
            CanState::ErrorPassive
        } else {
            CanState::ErrorActive
        }
    }

    pub const fn warning(&self) -> bool {
        self.tec >= Self::WARNING_LIMIT || self.rec >= Self::WARNING_LIMIT
    }

    pub const fn bus_off(&self) -> bool {
        self.bus_off
    }

    /// Counters as a driver reports them (8-bit; TEC saturates at 255 in bus-off).
    pub const fn tec(&self) -> u8 {
        if self.tec > 255 {
            255
        } else {
            self.tec as u8
        }
    }

    pub const fn rec(&self) -> u8 {
        if self.rec > 255 {
            255
        } else {
            self.rec as u8
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn transmit_errors_walk_through_passive_to_bus_off_and_successes_heal() {
        let mut m = ErrorCounterModel::new();
        for _ in 0..12 {
            m.apply(Event::TxError);
        }
        assert_eq!(m.tec(), 96);
        assert!(m.warning());
        assert_eq!(
            m.state(),
            CanState::ErrorActive,
            "warning is still error-active"
        );
        for _ in 0..4 {
            m.apply(Event::TxError);
        }
        assert_eq!(m.state(), CanState::ErrorPassive);
        for _ in 0..1 {
            m.apply(Event::TxSuccess);
        }
        assert_eq!(m.tec(), 127);
        assert_eq!(m.state(), CanState::ErrorActive);
        for _ in 0..32 {
            m.apply(Event::TxError);
        }
        assert!(m.bus_off());
        assert_eq!(m.state(), CanState::BusOff);
        assert_eq!(m.tec(), 255, "saturated for the driver's view");
        m.apply(Event::TxSuccess);
        assert!(m.bus_off(), "frozen until recovery");
        m.recover();
        assert_eq!((m.tec(), m.rec(), m.state()), (0, 0, CanState::ErrorActive));
    }

    #[test]
    fn receive_errors_saturate_above_passive_and_fall_back_to_127() {
        let mut m = ErrorCounterModel::new();
        for _ in 0..200 {
            m.apply(Event::RxError);
        }
        assert_eq!(m.rec(), 128);
        assert_eq!(m.state(), CanState::ErrorPassive);
        m.apply(Event::RxSuccess);
        assert_eq!(m.rec(), 127);
        assert_eq!(m.state(), CanState::ErrorActive);
        m.apply(Event::RxSuccess);
        assert_eq!(m.rec(), 126);
    }
}

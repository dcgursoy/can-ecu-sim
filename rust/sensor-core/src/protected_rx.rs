//! Receive-side validation of one protected message type, the port of
//! `ecu/protected_rx.hpp`: DLC, CRC and rolling counter, with statistics.
//!
//! Policy:
//!   wrong DLC          reject (the CRC cannot see truncation)
//!   CRC mismatch       reject
//!   counter repeated   reject (a duplicate or stale frame carries no new data)
//!   counter skipped    accept, count the lost frames (data is fresh)
//!   out of sequence    reject (late or replayed frame)
//! After the supervisor declares the message lost it calls `reset_sequence`,
//! so a sender that restarts with counter 0 is accepted instead of being
//! reported as out of sequence forever.

use core::marker::PhantomData;

use crate::e2e::{self, CounterStatus, CounterTracker};
use crate::messages::Message;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RxResult {
    Accepted,
    AcceptedWithLoss,
    RejectedDlc,
    RejectedCrc,
    RejectedRepeated,
    RejectedOutOfSequence,
}

impl RxResult {
    pub const fn accepted(self) -> bool {
        matches!(self, Self::Accepted | Self::AcceptedWithLoss)
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Accepted => "accepted",
            Self::AcceptedWithLoss => "accepted_with_loss",
            Self::RejectedDlc => "rejected_dlc",
            Self::RejectedCrc => "rejected_crc",
            Self::RejectedRepeated => "rejected_repeated",
            Self::RejectedOutOfSequence => "rejected_out_of_sequence",
        }
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct RxStats {
    pub accepted: u32,
    pub dlc_faults: u32,
    pub crc_faults: u32,
    pub repeated: u32,
    /// Frames accepted after a counter gap.
    pub skipped: u32,
    pub out_of_sequence: u32,
    /// Sum of counter gaps.
    pub lost_frames: u32,
    pub consecutive_rejects: u32,
}

impl RxStats {
    pub const fn faults(&self) -> u32 {
        self.dlc_faults + self.crc_faults + self.repeated + self.out_of_sequence
    }
}

#[derive(Clone, Copy, Debug)]
pub struct ProtectedReceiver<M: Message> {
    tracker: CounterTracker,
    stats: RxStats,
    _message: PhantomData<M>,
}

impl<M: Message> Default for ProtectedReceiver<M> {
    fn default() -> Self {
        Self::new()
    }
}

impl<M: Message> ProtectedReceiver<M> {
    pub const fn new() -> Self {
        Self {
            tracker: CounterTracker::new(),
            stats: RxStats {
                accepted: 0,
                dlc_faults: 0,
                crc_faults: 0,
                repeated: 0,
                skipped: 0,
                out_of_sequence: 0,
                lost_frames: 0,
                consecutive_rejects: 0,
            },
            _message: PhantomData,
        }
    }

    /// Validate a received buffer; the decoded message comes with an accepting result.
    pub fn receive(&mut self, data: &[u8]) -> (RxResult, Option<M>) {
        if data.len() != M::DLC {
            self.stats.dlc_faults = self.stats.dlc_faults.saturating_add(1);
            return (self.reject(RxResult::RejectedDlc), None);
        }
        if !e2e::crc_ok(M::ID, data) {
            self.stats.crc_faults = self.stats.crc_faults.saturating_add(1);
            return (self.reject(RxResult::RejectedCrc), None);
        }
        let lost_before = self.tracker.lost_total();
        match self.tracker.check(e2e::counter_of(data)) {
            CounterStatus::Repeated => {
                self.stats.repeated = self.stats.repeated.saturating_add(1);
                (self.reject(RxResult::RejectedRepeated), None)
            }
            CounterStatus::OutOfSequence => {
                self.stats.out_of_sequence = self.stats.out_of_sequence.saturating_add(1);
                (self.reject(RxResult::RejectedOutOfSequence), None)
            }
            CounterStatus::Skipped => {
                self.stats.skipped = self.stats.skipped.saturating_add(1);
                self.stats.lost_frames = self
                    .stats
                    .lost_frames
                    .saturating_add(self.tracker.lost_total() - lost_before);
                self.stats.accepted = self.stats.accepted.saturating_add(1);
                self.stats.consecutive_rejects = 0;
                (RxResult::AcceptedWithLoss, M::from_buffer(data))
            }
            CounterStatus::Ok => {
                self.stats.accepted = self.stats.accepted.saturating_add(1);
                self.stats.consecutive_rejects = 0;
                (RxResult::Accepted, M::from_buffer(data))
            }
        }
    }

    /// Forget the counter history (after a timeout, so a restarted sender is accepted).
    pub fn reset_sequence(&mut self) {
        self.tracker.reset();
    }

    pub const fn stats(&self) -> &RxStats {
        &self.stats
    }

    pub fn reset_stats(&mut self) {
        self.stats = RxStats::default();
    }

    fn reject(&mut self, r: RxResult) -> RxResult {
        self.stats.consecutive_rejects = self.stats.consecutive_rejects.saturating_add(1);
        r
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::messages::ControllerCommand;

    fn protected(counter: u8, torque: i16) -> [u8; 8] {
        let cmd = ControllerCommand {
            torque_cmd: torque,
            ..Default::default()
        };
        let mut frame = cmd.to_frame();
        e2e::protect_frame(ControllerCommand::ID, &mut frame, counter);
        frame
    }

    #[test]
    fn the_policy_end_to_end() {
        let mut rx: ProtectedReceiver<ControllerCommand> = ProtectedReceiver::new();
        let (r, msg) = rx.receive(&protected(0, 10));
        assert_eq!(r, RxResult::Accepted);
        assert_eq!(msg.unwrap().torque_cmd, 10);
        assert_eq!(rx.receive(&protected(1, 11)).0, RxResult::Accepted);
        assert_eq!(rx.receive(&protected(1, 11)).0, RxResult::RejectedRepeated);
        let (r, msg) = rx.receive(&protected(3, 13));
        assert_eq!(r, RxResult::AcceptedWithLoss);
        assert_eq!(msg.unwrap().torque_cmd, 13);
        assert_eq!(
            rx.receive(&protected(12, 1)).0,
            RxResult::RejectedOutOfSequence
        );
        let mut bad = protected(4, 14);
        bad[3] ^= 0x04;
        assert_eq!(rx.receive(&bad).0, RxResult::RejectedCrc);
        assert_eq!(rx.receive(&protected(4, 14)[..7]).0, RxResult::RejectedDlc);
        let s = *rx.stats();
        assert_eq!(
            (
                s.accepted,
                s.repeated,
                s.skipped,
                s.lost_frames,
                s.out_of_sequence,
                s.crc_faults,
                s.dlc_faults
            ),
            (3, 1, 1, 1, 1, 1, 1)
        );
        assert_eq!(s.faults(), 4);
        assert_eq!(
            s.consecutive_rejects, 3,
            "the out-of-sequence, CRC and DLC rejects in a row"
        );
        assert_eq!(rx.receive(&protected(4, 14)).0, RxResult::Accepted);
        assert_eq!(rx.stats().consecutive_rejects, 0);
        rx.reset_sequence();
        assert_eq!(
            rx.receive(&protected(0, 0)).0,
            RxResult::Accepted,
            "a restarted sender is accepted"
        );
        rx.reset_stats();
        assert_eq!(*rx.stats(), RxStats::default());
        assert!(RxResult::AcceptedWithLoss.accepted() && !RxResult::RejectedCrc.accepted());
        assert_eq!(
            RxResult::RejectedOutOfSequence.as_str(),
            "rejected_out_of_sequence"
        );
    }
}

//! End-to-end protection of safety-relevant messages: CRC-8 plus a 4-bit
//! rolling counter, the port of `protocol/crc8.hpp` and `protocol/e2e.hpp`.
//!
//! Layout, fixed by `can/network.dbc` and checked by the generators:
//!   byte 0           CRC-8H2F (poly 0x2F, init 0xFF, xor-out 0xFF, no
//!                    reflection) over `[id >> 8, id & 0xFF, bytes 1..DLC-1]`
//!   byte 1 bits 0-3  rolling counter, +1 per transmission, 15 wraps to 0
//!
//! Covering the 16-bit identifier means a frame delivered under the wrong
//! identifier fails the check even if its payload is intact. The counter lets
//! the receiver tell apart lost, repeated and out-of-order frames; the CRC
//! tells apart corrupted ones.

pub const POLYNOMIAL: u8 = 0x2F;
pub const INITIAL_VALUE: u8 = 0xFF;
pub const FINAL_XOR: u8 = 0xFF;
/// crc8("123456789")
pub const CHECK_VALUE: u8 = 0xDF;

pub const COUNTER_BITS: u32 = 4;
pub const COUNTER_MASK: u8 = (1 << COUNTER_BITS) - 1;
/// Counter deltas 2..=MAX_LOST_TOLERATED+1 count as lost frames; larger deltas
/// are an out-of-sequence (late or replayed) frame.
pub const MAX_LOST_TOLERATED: u8 = 7;

/// One byte through the bitwise definition (used to build the table and in tests).
pub const fn update_bitwise(crc: u8, byte: u8) -> u8 {
    let mut crc = crc ^ byte;
    let mut bit = 0;
    while bit < 8 {
        let msb = crc & 0x80 != 0;
        crc <<= 1;
        if msb {
            crc ^= POLYNOMIAL;
        }
        bit += 1;
    }
    crc
}

const fn make_table() -> [u8; 256] {
    let mut table = [0u8; 256];
    let mut i = 0;
    while i < 256 {
        table[i] = update_bitwise(0, i as u8);
        i += 1;
    }
    table
}

/// The lookup table, built at compile time from the bitwise definition.
pub const TABLE: [u8; 256] = make_table();

/// Incremental CRC: feed bytes with `update`, read the result with `value`.
#[derive(Clone, Copy, Debug)]
pub struct Crc8 {
    crc: u8,
}

impl Default for Crc8 {
    fn default() -> Self {
        Self::new()
    }
}

impl Crc8 {
    pub const fn new() -> Self {
        Self { crc: INITIAL_VALUE }
    }

    pub const fn update(&mut self, byte: u8) {
        self.crc = TABLE[(self.crc ^ byte) as usize];
    }

    pub const fn update_slice(&mut self, data: &[u8]) {
        let mut i = 0;
        while i < data.len() {
            self.update(data[i]);
            i += 1;
        }
    }

    /// Final CRC (final XOR applied). The object can keep being updated.
    pub const fn value(&self) -> u8 {
        self.crc ^ FINAL_XOR
    }

    pub const fn reset(&mut self) {
        self.crc = INITIAL_VALUE;
    }
}

/// CRC of a complete buffer.
pub const fn compute(data: &[u8]) -> u8 {
    let mut crc = Crc8::new();
    crc.update_slice(data);
    crc.value()
}

/// CRC of a protected frame: 16-bit big-endian identifier, then bytes 1..end.
pub const fn frame_crc(id: u16, frame: &[u8]) -> u8 {
    let mut crc = Crc8::new();
    crc.update((id >> 8) as u8);
    crc.update((id & 0xFF) as u8);
    let mut i = 1;
    while i < frame.len() {
        crc.update(frame[i]);
        i += 1;
    }
    crc.value()
}

/// CRC check of a received frame (the caller checks the DLC first: the CRC
/// cannot see truncation).
pub const fn crc_ok(id: u16, frame: &[u8]) -> bool {
    frame.len() >= 2 && frame_crc(id, frame) == frame[0]
}

pub const fn counter_of(frame: &[u8]) -> u8 {
    frame[1] & COUNTER_MASK
}

/// Write the counter into byte 1 and a valid CRC into byte 0 of an encoded
/// frame (the sender side of `e2e_ref.protect`).
pub const fn protect_frame(id: u16, frame: &mut [u8; 8], counter: u8) {
    frame[1] = (frame[1] & !COUNTER_MASK) | (counter & COUNTER_MASK);
    frame[0] = frame_crc(id, frame);
}

/// Sender side: 0, 1, ..., 15, 0, ...
#[derive(Clone, Copy, Debug, Default)]
pub struct CounterGenerator {
    value: u8,
}

impl CounterGenerator {
    pub const fn new() -> Self {
        Self { value: 0 }
    }

    pub fn advance(&mut self) -> u8 {
        let value = self.value;
        self.value = (self.value + 1) & COUNTER_MASK;
        value
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CounterStatus {
    /// delta 1, or the first frame seen
    Ok,
    /// delta 0: same counter twice (stale or duplicated frame)
    Repeated,
    /// delta 2..=MAX_LOST_TOLERATED+1: frames were lost
    Skipped,
    /// larger delta: late or replayed frame
    OutOfSequence,
}

impl CounterStatus {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Ok => "ok",
            Self::Repeated => "repeated",
            Self::Skipped => "skipped",
            Self::OutOfSequence => "out_of_sequence",
        }
    }
}

/// Receiver side: classifies each received counter against the last accepted
/// one. Ok, repeated and skipped frames move the reference forward; an
/// out-of-sequence frame is reported but does not move it, so a lone late
/// frame costs exactly one fault and the stream resumes with "ok".
#[derive(Clone, Copy, Debug, Default)]
pub struct CounterTracker {
    has_last: bool,
    last: u8,
    lost_total: u32,
}

impl CounterTracker {
    pub const fn new() -> Self {
        Self {
            has_last: false,
            last: 0,
            lost_total: 0,
        }
    }

    pub fn check(&mut self, counter: u8) -> CounterStatus {
        let counter = counter & COUNTER_MASK;
        if !self.has_last {
            self.has_last = true;
            self.last = counter;
            return CounterStatus::Ok;
        }
        let delta = counter.wrapping_sub(self.last) & COUNTER_MASK;
        if delta == 0 {
            return CounterStatus::Repeated;
        }
        if delta == 1 {
            self.last = counter;
            return CounterStatus::Ok;
        }
        if delta <= MAX_LOST_TOLERATED + 1 {
            self.last = counter;
            self.lost_total = self.lost_total.saturating_add(u32::from(delta - 1));
            return CounterStatus::Skipped;
        }
        CounterStatus::OutOfSequence
    }

    pub fn reset(&mut self) {
        self.has_last = false;
        self.last = 0;
    }

    pub const fn lost_total(&self) -> u32 {
        self.lost_total
    }

    pub const fn has_last(&self) -> bool {
        self.has_last
    }

    pub const fn last(&self) -> u8 {
        self.last
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check_value_and_autosar_vectors() {
        assert_eq!(compute(b"123456789"), CHECK_VALUE);
        let cases: [(&[u8], u8); 7] = [
            (&[0x00, 0x00, 0x00, 0x00], 0x12),
            (&[0xF2, 0x01, 0x83], 0xC2),
            (&[0x0F, 0xAA, 0x00, 0x55], 0xC6),
            (&[0x00, 0xFF, 0x55, 0x11], 0x77),
            (
                &[0x33, 0x22, 0x55, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF],
                0x11,
            ),
            (&[0x92, 0x6B, 0x55], 0x33),
            (&[0xFF, 0xFF, 0xFF, 0xFF], 0x6C),
        ];
        for (data, crc) in cases {
            assert_eq!(compute(data), crc);
        }
    }

    #[test]
    fn table_matches_the_bitwise_definition() {
        for i in 0..256u32 {
            assert_eq!(TABLE[i as usize], update_bitwise(0, i as u8));
        }
    }

    #[test]
    fn protect_then_check_and_misrouting_is_detected() {
        let mut frame = [0u8, 0xA5, 1, 2, 3, 4, 5, 6];
        protect_frame(0x100, &mut frame, 0x19); // 0x19 & 0xF == 9
        assert!(crc_ok(0x100, &frame));
        assert_eq!(counter_of(&frame), 9);
        assert_eq!(
            frame[1] & 0xF0,
            0xA0,
            "the upper nibble of byte 1 is payload"
        );
        assert!(!crc_ok(0x101, &frame), "the identifier is covered");
        let mut bad = frame;
        bad[5] ^= 0x10;
        assert!(!crc_ok(0x100, &bad));
        assert!(!crc_ok(0x100, &frame[..1]), "too short for a CRC check");
    }

    #[test]
    fn counter_generator_wraps() {
        let mut gen = CounterGenerator::new();
        for expected in 0..16u8 {
            assert_eq!(gen.advance(), expected);
        }
        assert_eq!(gen.advance(), 0);
    }

    #[test]
    fn counter_tracker_policy() {
        let mut t = CounterTracker::new();
        assert!(!t.has_last());
        assert_eq!(t.check(3), CounterStatus::Ok);
        assert_eq!(t.check(4), CounterStatus::Ok);
        assert_eq!(t.check(4), CounterStatus::Repeated);
        assert_eq!(t.check(6), CounterStatus::Skipped);
        assert_eq!(t.lost_total(), 1);
        assert_eq!(t.check(15), CounterStatus::OutOfSequence);
        assert_eq!(t.last(), 6);
        assert_eq!(t.check(7), CounterStatus::Ok);
        assert_eq!(t.check(15), CounterStatus::Skipped);
        assert_eq!(t.lost_total(), 8);
        assert_eq!(t.check(0), CounterStatus::Ok);
        assert_eq!(t.check(15), CounterStatus::OutOfSequence);
        t.reset();
        assert!(!t.has_last());
        assert_eq!(t.check(11), CounterStatus::Ok);
        assert_eq!(CounterStatus::Skipped.as_str(), "skipped");
    }

    #[test]
    fn a_late_frame_does_not_produce_a_phantom_loss() {
        let mut t = CounterTracker::new();
        for c in [5, 6, 7] {
            assert_eq!(t.check(c), CounterStatus::Ok);
        }
        assert_eq!(t.check(2), CounterStatus::OutOfSequence);
        assert_eq!(t.check(8), CounterStatus::Ok);
        assert_eq!(t.lost_total(), 0);
        assert_eq!(
            t.check(0x1A),
            CounterStatus::Skipped,
            "the upper bits are ignored: 0x1A is counter 10, two after 8"
        );
        assert_eq!(t.lost_total(), 1);
    }
}

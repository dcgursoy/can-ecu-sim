//! Bit-level helpers used by the generated codec (`messages.rs`), the port of
//! `firmware/lib/protocol/include/protocol/codec_util.hpp`.
//!
//! Signals are little-endian (Intel) as in the DBC: the start bit is the
//! position of the signal's least significant bit, counted from bit 0 of byte
//! 0, and the signal continues towards higher bit positions. The loops are
//! deliberately simple so the generated code is obviously correct; the
//! conformance tests check them bit for bit against cantools' encoding.

/// Read an unsigned field of `length` bits starting at `start_bit`.
pub const fn get_unsigned(d: &[u8; 8], start_bit: u32, length: u32) -> u32 {
    let mut value: u32 = 0;
    let mut i = 0;
    while i < length {
        let bit = start_bit + i;
        if (d[(bit / 8) as usize] >> (bit % 8)) & 1 != 0 {
            value |= 1 << i;
        }
        i += 1;
    }
    value
}

/// Read a signed (two's complement) field of `length` bits.
pub const fn get_signed(d: &[u8; 8], start_bit: u32, length: u32) -> i32 {
    let raw = get_unsigned(d, start_bit, length);
    if length < 32 && raw & (1 << (length - 1)) != 0 {
        (raw | (u32::MAX << length)) as i32
    } else {
        raw as i32
    }
}

/// Write the low `length` bits of `value` at `start_bit`, clearing the old bits.
pub const fn put(d: &mut [u8; 8], start_bit: u32, length: u32, value: u32) {
    let mut i = 0;
    while i < length {
        let bit = start_bit + i;
        let mask: u8 = 1 << (bit % 8);
        if (value >> i) & 1 != 0 {
            d[(bit / 8) as usize] |= mask;
        } else {
            d[(bit / 8) as usize] &= !mask;
        }
        i += 1;
    }
}

/// raw -> physical: `raw * scale + offset`.
pub fn raw_to_phys(raw: i64, scale: f32, offset: f32) -> f32 {
    raw as f32 * scale + offset
}

/// physical -> raw, rounded to the nearest step (halves away from zero) and
/// saturated to `[raw_min, raw_max]`. NaN maps to `raw_min`: a NaN physical
/// value is a computation fault upstream, and the lowest raw value is the one
/// every signal in this network treats as its most conservative reading. The
/// arithmetic is the C++ one step for step, so both sides encode identical
/// raw values for identical inputs.
pub fn phys_to_raw(phys: f32, scale: f32, offset: f32, raw_min: i64, raw_max: i64) -> i64 {
    let steps = (phys - offset) / scale;
    if steps.is_nan() {
        return raw_min;
    }
    // Saturate in float first so the conversion to an integer below is always in range.
    let lo = raw_min as f32;
    let hi = raw_max as f32;
    if steps <= lo {
        return raw_min;
    }
    if steps >= hi {
        return raw_max;
    }
    // Round in two exact steps: truncate, then look at the remainder. Adding
    // 0.5 before truncating would round twice (once in the float addition).
    let truncated = steps as i64; // `as` truncates toward zero, like the C++ cast
    let remainder = steps - truncated as f32;
    let mut rounded = truncated;
    if remainder >= 0.5 {
        rounded += 1;
    } else if remainder <= -0.5 {
        rounded -= 1;
    }
    rounded.clamp(raw_min, raw_max)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn put_then_get_round_trips_across_byte_boundaries() {
        let mut d = [0u8; 8];
        put(&mut d, 12, 4, 0xA);
        put(&mut d, 16, 16, 0xBEEF);
        put(&mut d, 50, 6, 0x2A);
        assert_eq!(get_unsigned(&d, 12, 4), 0xA);
        assert_eq!(get_unsigned(&d, 16, 16), 0xBEEF);
        assert_eq!(get_unsigned(&d, 50, 6), 0x2A);
        assert_eq!(d[0], 0, "untouched bits stay zero");
        assert_eq!(get_unsigned(&d, 8, 4), 0);
    }

    #[test]
    fn put_masks_to_the_field_width_and_clears_old_bits() {
        let mut d = [0xFFu8; 8];
        put(&mut d, 8, 4, 0x1F5); // only the low nibble lands
        assert_eq!(get_unsigned(&d, 8, 4), 0x5);
        assert_eq!(
            get_unsigned(&d, 12, 4),
            0xF,
            "the neighbouring nibble is untouched"
        );
    }

    #[test]
    fn signed_fields_sign_extend() {
        let mut d = [0u8; 8];
        put(&mut d, 32, 16, (-1234i16) as u16 as u32);
        assert_eq!(get_signed(&d, 32, 16), -1234);
        put(&mut d, 0, 8, 0x80);
        assert_eq!(get_signed(&d, 0, 8), -128);
        put(&mut d, 0, 8, 0x7F);
        assert_eq!(get_signed(&d, 0, 8), 127);
        let mut e = [0u8; 8];
        put(&mut e, 0, 32, 0xFFFF_FFFF);
        assert_eq!(get_signed(&e, 0, 32), -1);
    }

    #[test]
    fn physical_conversion_rounds_and_saturates() {
        assert_eq!(phys_to_raw(12.34, 0.01, 0.0, 0, 65535), 1234);
        assert_eq!(phys_to_raw(12.344, 0.01, 0.0, 0, 65535), 1234);
        assert_eq!(phys_to_raw(12.346, 0.01, 0.0, 0, 65535), 1235);
        assert_eq!(phys_to_raw(-12.346, 0.01, 0.0, -32768, 32767), -1235);
        assert_eq!(phys_to_raw(1.0e9, 0.01, 0.0, 0, 65535), 65535);
        assert_eq!(phys_to_raw(-1.0, 0.01, 0.0, 0, 65535), 0);
        assert_eq!(phys_to_raw(-1.0e9, 0.01, 0.0, -32768, 32767), -32768);
        assert_eq!(phys_to_raw(25.0, 1.0, -40.0, 0, 255), 65);
        assert_eq!(
            phys_to_raw(0.5, 1.0, 0.0, 0, 255),
            1,
            "halves away from zero"
        );
        assert_eq!(phys_to_raw(-2.5, 1.0, 0.0, -128, 127), -3);
        assert_eq!(phys_to_raw(f32::NAN, 0.01, 0.0, -32768, 32767), -32768);
        assert_eq!(phys_to_raw(f32::INFINITY, 0.01, 0.0, 0, 65535), 65535);
        assert!((raw_to_phys(65, 1.0, -40.0) - 25.0).abs() < 1e-6);
    }
}

// Bit-level helpers used by the generated codec (protocol/messages.hpp).
//
// Signals are little-endian (Intel) as in the DBC: the start bit is the
// position of the signal's least significant bit, counted from bit 0 of byte 0,
// and the signal continues towards higher bit positions. The loops below are
// deliberately simple so the generated code is obviously correct; the unit
// tests check them bit for bit against cantools.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace proto::codec {

/// Read an unsigned field of `length` bits starting at `start_bit`.
template <std::size_t N>
constexpr std::uint32_t get_unsigned(const std::array<std::uint8_t, N>& d, unsigned start_bit, unsigned length)
{
    std::uint32_t value = 0;
    for (unsigned i = 0; i < length; ++i) {
        const unsigned pos = start_bit + i;
        const std::uint32_t bit = (d[pos / 8U] >> (pos % 8U)) & 1U;
        value |= bit << i;
    }
    return value;
}

/// Read a two's-complement signed field of `length` bits (1..32).
template <std::size_t N>
constexpr std::int32_t get_signed(const std::array<std::uint8_t, N>& d, unsigned start_bit, unsigned length)
{
    const std::uint32_t raw = get_unsigned(d, start_bit, length);
    if (length >= 32U) {
        return static_cast<std::int32_t>(raw);
    }
    const std::uint32_t sign = 1U << (length - 1U);
    if ((raw & sign) == 0U) {
        return static_cast<std::int32_t>(raw);
    }
    // Negative: raw - 2^length, computed without signed overflow.
    const std::uint32_t magnitude = (1U << length) - raw;
    return -static_cast<std::int32_t>(magnitude);
}

/// Write the low `length` bits of `value` at `start_bit`; higher bits are ignored.
template <std::size_t N>
constexpr void put(std::array<std::uint8_t, N>& d, unsigned start_bit, unsigned length, std::uint32_t value)
{
    for (unsigned i = 0; i < length; ++i) {
        const unsigned pos = start_bit + i;
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << (pos % 8U));
        if (((value >> i) & 1U) != 0U) {
            d[pos / 8U] = static_cast<std::uint8_t>(d[pos / 8U] | mask);
        } else {
            d[pos / 8U] = static_cast<std::uint8_t>(d[pos / 8U] & static_cast<std::uint8_t>(~mask));
        }
    }
}

/// raw -> physical: phys = raw * scale + offset.
constexpr float raw_to_phys(std::int32_t raw, float scale, float offset)
{
    return static_cast<float>(raw) * scale + offset;
}

/// physical -> raw, rounded to the nearest step (halves away from zero) and
/// saturated to [raw_min, raw_max]. NaN maps to raw_min: a NaN physical value
/// is a computation fault upstream, and the lowest raw value is the one every
/// signal in this network treats as its most conservative reading.
constexpr std::int32_t phys_to_raw(float phys, float scale, float offset, std::int32_t raw_min, std::int32_t raw_max)
{
    const float steps = (phys - offset) / scale;
    if (steps != steps) {  // NaN (no <cmath> in constexpr context)
        return raw_min;
    }
    // Saturate in float first so the conversion to int32 below is always in range.
    const float lo = static_cast<float>(raw_min);
    const float hi = static_cast<float>(raw_max);
    if (steps <= lo) {
        return raw_min;
    }
    if (steps >= hi) {
        return raw_max;
    }
    // Round in two exact steps: truncate, then look at the remainder. Adding
    // 0.5F before truncating would round twice (once in the float addition).
    const std::int32_t truncated = static_cast<std::int32_t>(steps);
    const float remainder = steps - static_cast<float>(truncated);
    std::int32_t rounded = truncated;
    if (remainder >= 0.5F) {
        ++rounded;
    } else if (remainder <= -0.5F) {
        --rounded;
    }
    return rounded < raw_min ? raw_min : (rounded > raw_max ? raw_max : rounded);
}

}  // namespace proto::codec

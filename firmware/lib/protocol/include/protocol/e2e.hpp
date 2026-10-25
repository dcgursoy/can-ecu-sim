// End-to-end protection of safety-relevant messages: CRC-8 plus a 4-bit rolling
// counter, modelled on AUTOSAR E2E profile 1 (layout, identifier in the CRC,
// counter width) and profile 2 (polynomial); not an implementation of either.
//
// Layout, fixed by can/network.dbc and checked by can/gen_codec.py:
//   byte 0           CRC-8H2F (poly 0x2F) over [id >> 8, id & 0xFF, bytes 1..DLC-1]
//   byte 1 bits 0-3  rolling counter, +1 per transmission, 15 wraps to 0
//
// Covering the 16-bit identifier ("data ID") means a frame delivered under the
// wrong identifier, e.g. by a misconfigured gateway, fails the check even if its
// payload is intact. The counter lets the receiver tell apart lost, repeated and
// out-of-order frames; the CRC tells apart corrupted ones.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol/crc8.hpp"

namespace proto::e2e {

constexpr unsigned kCounterBits = 4;
constexpr std::uint8_t kCounterMask = (1U << kCounterBits) - 1U;
/// Counter deltas 2..kMaxLostTolerated+1 count as lost frames; larger deltas are
/// treated as an out-of-sequence (late or replayed) frame.
constexpr std::uint8_t kMaxLostTolerated = 7;

/// CRC of a protected frame: 16-bit big-endian identifier, then bytes 1..N-1.
template <std::size_t N>
constexpr std::uint8_t compute_crc(std::uint32_t id, const std::array<std::uint8_t, N>& frame)
{
    static_assert(N >= 2, "a protected frame needs a CRC byte and a counter byte");
    crc8::Crc8 crc;
    crc.update(static_cast<std::uint8_t>((id >> 8U) & 0xFFU));
    crc.update(static_cast<std::uint8_t>(id & 0xFFU));
    for (std::size_t i = 1; i < N; ++i) {
        crc.update(frame[i]);
    }
    return crc.value();
}

template <std::size_t N>
constexpr bool crc_ok(std::uint32_t id, const std::array<std::uint8_t, N>& frame)
{
    return compute_crc(id, frame) == frame[0];
}

template <std::size_t N>
constexpr std::uint8_t counter_of(const std::array<std::uint8_t, N>& frame)
{
    return static_cast<std::uint8_t>(frame[1] & kCounterMask);
}

/// Pack a message with the given counter and a valid CRC. Works for any
/// generated message type with kSafetyRelevant == true (crc and counter fields).
template <class Msg>
constexpr typename Msg::Frame protect(Msg msg, std::uint8_t counter)
{
    static_assert(Msg::kSafetyRelevant, "protect() is for safety-relevant messages only");
    msg.counter = static_cast<decltype(msg.counter)>(counter & kCounterMask);
    msg.crc = 0;
    typename Msg::Frame frame = msg.to_frame();
    frame[0] = compute_crc(Msg::kId, frame);
    return frame;
}

/// Sender side: 0, 1, ..., 15, 0, ...
class CounterGenerator {
public:
    constexpr std::uint8_t next()
    {
        const std::uint8_t value = value_;
        value_ = static_cast<std::uint8_t>((value_ + 1U) & kCounterMask);
        return value;
    }

private:
    std::uint8_t value_ = 0;
};

enum class CounterStatus : std::uint8_t {
    kOk,             ///< delta 1, or the first frame seen
    kRepeated,       ///< delta 0: same counter twice (stale or duplicated frame)
    kSkipped,        ///< delta 2..kMaxLostTolerated+1: frames were lost
    kOutOfSequence,  ///< larger delta: late or replayed frame
};

constexpr const char* to_string(CounterStatus status)
{
    switch (status) {
    case CounterStatus::kOk: return "ok";
    case CounterStatus::kRepeated: return "repeated";
    case CounterStatus::kSkipped: return "skipped";
    case CounterStatus::kOutOfSequence: return "out_of_sequence";
    }
    return "unknown";
}

/// Receiver side: classifies each received counter against the last accepted
/// one. Ok, repeated and skipped frames move the reference forward; an
/// out-of-sequence (late or replayed) frame is reported but does not move it,
/// so a lone late frame costs exactly one fault and the stream resumes with
/// "ok". Counting and reacting to faults is the supervisor's job.
class CounterTracker {
public:
    constexpr CounterStatus check(std::uint8_t counter)
    {
        counter = static_cast<std::uint8_t>(counter & kCounterMask);
        if (!has_last_) {
            has_last_ = true;
            last_ = counter;
            return CounterStatus::kOk;
        }
        const std::uint8_t delta = static_cast<std::uint8_t>((counter - last_) & kCounterMask);
        if (delta == 0U) {
            return CounterStatus::kRepeated;
        }
        if (delta == 1U) {
            last_ = counter;
            return CounterStatus::kOk;
        }
        if (delta <= kMaxLostTolerated + 1U) {
            last_ = counter;
            lost_total_ += static_cast<std::uint32_t>(delta - 1U);
            return CounterStatus::kSkipped;
        }
        return CounterStatus::kOutOfSequence;
    }

    constexpr void reset()
    {
        has_last_ = false;
        last_ = 0;
    }

    constexpr std::uint32_t lost_total() const { return lost_total_; }
    constexpr bool has_last() const { return has_last_; }
    constexpr std::uint8_t last() const { return last_; }

private:
    bool has_last_ = false;
    std::uint8_t last_ = 0;
    std::uint32_t lost_total_ = 0;
};

}  // namespace proto::e2e

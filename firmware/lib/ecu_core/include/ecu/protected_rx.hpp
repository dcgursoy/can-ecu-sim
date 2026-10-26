// Receive-side validation of one protected message type: DLC, CRC and rolling
// counter, with statistics. Wraps the protocol library's E2E primitives into
// the acceptance policy the ECUs use.
//
// Policy:
//   wrong DLC          reject (the CRC cannot see truncation)
//   CRC mismatch       reject
//   counter repeated   reject (a duplicate or stale frame carries no new data)
//   counter skipped    accept, count the lost frames (data is fresh)
//   out of sequence    reject (late or replayed frame)
// After the supervisor declares the message lost it calls reset_sequence(), so
// a sender that restarts with counter 0 is accepted instead of being reported
// as out of sequence forever.
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol/e2e.hpp"

namespace ecu {

enum class RxResult : std::uint8_t {
    kAccepted,
    kAcceptedWithLoss,
    kRejectedDlc,
    kRejectedCrc,
    kRejectedRepeated,
    kRejectedOutOfSequence,
};

constexpr const char* to_string(RxResult r)
{
    switch (r) {
    case RxResult::kAccepted: return "accepted";
    case RxResult::kAcceptedWithLoss: return "accepted_with_loss";
    case RxResult::kRejectedDlc: return "rejected_dlc";
    case RxResult::kRejectedCrc: return "rejected_crc";
    case RxResult::kRejectedRepeated: return "rejected_repeated";
    case RxResult::kRejectedOutOfSequence: return "rejected_out_of_sequence";
    }
    return "unknown";
}

constexpr bool accepted(RxResult r)
{
    return r == RxResult::kAccepted || r == RxResult::kAcceptedWithLoss;
}

struct RxStats {
    std::uint32_t accepted = 0;
    std::uint32_t dlc_faults = 0;
    std::uint32_t crc_faults = 0;
    std::uint32_t repeated = 0;
    std::uint32_t skipped = 0;        ///< frames accepted after a counter gap
    std::uint32_t out_of_sequence = 0;
    std::uint32_t lost_frames = 0;    ///< sum of counter gaps
    std::uint32_t consecutive_rejects = 0;

    constexpr std::uint32_t faults() const
    {
        return dlc_faults + crc_faults + repeated + out_of_sequence;
    }
};

template <class Msg>
class ProtectedReceiver {
    static_assert(Msg::kSafetyRelevant, "ProtectedReceiver is for safety-relevant messages");

public:
    /// Validate a received buffer; on acceptance `out` holds the decoded message.
    constexpr RxResult receive(const std::uint8_t* data, std::size_t len, Msg& out)
    {
        if (data == nullptr || len != Msg::kDlc) {
            ++stats_.dlc_faults;
            return reject(RxResult::kRejectedDlc);
        }
        typename Msg::Frame frame{};
        for (std::size_t i = 0; i < Msg::kDlc; ++i) {
            frame[i] = data[i];
        }
        if (!proto::e2e::crc_ok(Msg::kId, frame)) {
            ++stats_.crc_faults;
            return reject(RxResult::kRejectedCrc);
        }
        const std::uint32_t lost_before = tracker_.lost_total();
        switch (tracker_.check(proto::e2e::counter_of(frame))) {
        case proto::e2e::CounterStatus::kRepeated:
            ++stats_.repeated;
            return reject(RxResult::kRejectedRepeated);
        case proto::e2e::CounterStatus::kOutOfSequence:
            ++stats_.out_of_sequence;
            return reject(RxResult::kRejectedOutOfSequence);
        case proto::e2e::CounterStatus::kSkipped:
            ++stats_.skipped;
            stats_.lost_frames += tracker_.lost_total() - lost_before;
            out = Msg::from_frame(frame);
            ++stats_.accepted;
            stats_.consecutive_rejects = 0;
            return RxResult::kAcceptedWithLoss;
        case proto::e2e::CounterStatus::kOk:
            break;
        }
        out = Msg::from_frame(frame);
        ++stats_.accepted;
        stats_.consecutive_rejects = 0;
        return RxResult::kAccepted;
    }

    /// Forget the counter history (after a timeout, so a restarted sender is accepted).
    constexpr void reset_sequence() { tracker_.reset(); }

    constexpr const RxStats& stats() const { return stats_; }
    constexpr void reset_stats() { stats_ = RxStats{}; }

private:
    constexpr RxResult reject(RxResult r)
    {
        ++stats_.consecutive_rejects;
        return r;
    }

    proto::e2e::CounterTracker tracker_{};
    RxStats stats_{};
};

}  // namespace ecu

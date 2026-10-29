#include <cstdint>

#include <gtest/gtest.h>

#include "ecu/protected_rx.hpp"
#include "protocol/messages.hpp"

namespace {

using proto::SensorData;
using Receiver = ecu::ProtectedReceiver<SensorData>;

SensorData::Frame protected_frame(std::uint8_t counter, std::uint16_t speed)
{
    SensorData m{};
    m.speed = speed;
    return proto::e2e::protect(m, counter);
}

TEST(ProtectedReceiver, AcceptsValidSequence)
{
    Receiver rx;
    SensorData out{};
    for (std::uint8_t c = 0; c < 20; ++c) {
        const auto f = protected_frame(c, static_cast<std::uint16_t>(100 + c));
        EXPECT_EQ(rx.receive(f.data(), f.size(), out), ecu::RxResult::kAccepted);
        EXPECT_EQ(out.speed, 100 + c);
    }
    EXPECT_EQ(rx.stats().accepted, 20U);
    EXPECT_EQ(rx.stats().faults(), 0U);
}

TEST(ProtectedReceiver, RejectsWrongDlcCrcRepeatAndOutOfSequence)
{
    Receiver rx;
    SensorData out{};
    auto f = protected_frame(3, 500);
    EXPECT_EQ(rx.receive(f.data(), f.size() - 1, out), ecu::RxResult::kRejectedDlc);
    EXPECT_EQ(rx.receive(nullptr, f.size(), out), ecu::RxResult::kRejectedDlc);
    EXPECT_EQ(rx.receive(f.data(), f.size(), out), ecu::RxResult::kAccepted);

    auto corrupted = protected_frame(4, 501);
    corrupted[5] ^= 0x10;
    EXPECT_EQ(rx.receive(corrupted.data(), corrupted.size(), out), ecu::RxResult::kRejectedCrc);
    EXPECT_EQ(out.speed, 500) << "a rejected frame must not modify the output";

    EXPECT_EQ(rx.receive(f.data(), f.size(), out), ecu::RxResult::kRejectedRepeated);
    const auto late = protected_frame(14, 7);  // delta 11 from 3
    EXPECT_EQ(rx.receive(late.data(), late.size(), out), ecu::RxResult::kRejectedOutOfSequence);
    const auto next = protected_frame(4, 502);
    EXPECT_EQ(rx.receive(next.data(), next.size(), out), ecu::RxResult::kAccepted);

    const auto& s = rx.stats();
    EXPECT_EQ(s.dlc_faults, 2U);
    EXPECT_EQ(s.crc_faults, 1U);
    EXPECT_EQ(s.repeated, 1U);
    EXPECT_EQ(s.out_of_sequence, 1U);
    EXPECT_EQ(s.accepted, 2U);
    EXPECT_EQ(s.faults(), 5U);
    EXPECT_EQ(s.consecutive_rejects, 0U);
}

TEST(ProtectedReceiver, SkippedCounterIsAcceptedAndLossCounted)
{
    Receiver rx;
    SensorData out{};
    auto f = protected_frame(0, 1);
    rx.receive(f.data(), f.size(), out);
    f = protected_frame(3, 2);  // counters 1 and 2 lost
    EXPECT_EQ(rx.receive(f.data(), f.size(), out), ecu::RxResult::kAcceptedWithLoss);
    EXPECT_EQ(out.speed, 2);
    EXPECT_EQ(rx.stats().skipped, 1U);
    EXPECT_EQ(rx.stats().lost_frames, 2U);
    EXPECT_TRUE(ecu::accepted(ecu::RxResult::kAcceptedWithLoss));
    EXPECT_FALSE(ecu::accepted(ecu::RxResult::kRejectedCrc));
}

TEST(ProtectedReceiver, ConsecutiveRejectsAndSequenceReset)
{
    Receiver rx;
    SensorData out{};
    auto f = protected_frame(3, 1);
    rx.receive(f.data(), f.size(), out);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(rx.receive(f.data(), f.size(), out), ecu::RxResult::kRejectedRepeated);
    }
    EXPECT_EQ(rx.stats().consecutive_rejects, 3U);

    // A restarted sender begins at 0 (delta 13 from 3): out of sequence until the
    // supervisor, having declared the message lost, resets the receiver.
    const auto restart = protected_frame(0, 5);
    EXPECT_EQ(rx.receive(restart.data(), restart.size(), out), ecu::RxResult::kRejectedOutOfSequence);
    rx.reset_sequence();
    EXPECT_EQ(rx.receive(restart.data(), restart.size(), out), ecu::RxResult::kAccepted);
    EXPECT_EQ(rx.stats().consecutive_rejects, 0U);
    rx.reset_stats();
    EXPECT_EQ(rx.stats().accepted, 0U);
}

TEST(ProtectedReceiver, ResultNames)
{
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kAccepted), "accepted");
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kAcceptedWithLoss), "accepted_with_loss");
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kRejectedDlc), "rejected_dlc");
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kRejectedCrc), "rejected_crc");
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kRejectedRepeated), "rejected_repeated");
    EXPECT_STREQ(ecu::to_string(ecu::RxResult::kRejectedOutOfSequence), "rejected_out_of_sequence");
    EXPECT_STREQ(ecu::to_string(static_cast<ecu::RxResult>(99)), "unknown");  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange): the out-of-table value is the point
}

}  // namespace

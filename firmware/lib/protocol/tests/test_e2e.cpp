#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "generated/test_vectors.hpp"
#include "protocol/e2e.hpp"
#include "protocol/messages.hpp"

namespace {

using proto::e2e::CounterStatus;
using proto::e2e::CounterTracker;

// The Python reference produced protected frames for every safety-relevant
// message; protecting the same plain frame in C++ must give identical bytes.
TEST(E2e, ProtectMatchesPythonReference)
{
    for (const auto& v : vectors::kE2e) {
        auto frame = v.plain;
        frame[1] = static_cast<std::uint8_t>((frame[1] & 0xF0U) | (v.counter & 0x0FU));
        frame[0] = proto::e2e::compute_crc(v.id, frame);
        EXPECT_EQ(frame, v.protected_frame) << "id 0x" << std::hex << v.id << " counter " << int(v.counter);
        EXPECT_TRUE(proto::e2e::crc_ok(v.id, v.protected_frame));
        EXPECT_EQ(proto::e2e::counter_of(v.protected_frame), v.counter);
    }
}

TEST(E2e, ProtectTemplateSetsCounterAndCrcAndKeepsPayload)
{
    proto::SensorData msg{};
    msg.speed = 1234;
    msg.yaw_rate = -56;
    msg.accel_x = 78;
    msg.sensor_status = proto::SensorStatus::kDegraded;
    msg.counter = 0xFF;  // out of range on purpose: protect() masks it
    msg.crc = 0xAA;      // ignored: protect() recomputes it

    const auto frame = proto::e2e::protect(msg, 0x19);  // 0x19 & 0xF == 9
    EXPECT_TRUE(proto::e2e::crc_ok(proto::SensorData::kId, frame));
    EXPECT_EQ(proto::e2e::counter_of(frame), 9);

    const auto back = proto::SensorData::from_frame(frame);
    EXPECT_EQ(back.speed, 1234);
    EXPECT_EQ(back.yaw_rate, -56);
    EXPECT_EQ(back.accel_x, 78);
    EXPECT_EQ(back.sensor_status, proto::SensorStatus::kDegraded);
    EXPECT_EQ(back.counter, 9);
    EXPECT_EQ(back.crc, frame[0]);
}

// Every single-bit error in the covered bytes and in the identifier is caught.
TEST(E2e, EverySingleBitErrorIsDetected)
{
    for (const auto& v : vectors::kE2e) {
        for (unsigned bit = 8; bit < 64; ++bit) {
            auto corrupted = v.protected_frame;
            corrupted[bit / 8] = static_cast<std::uint8_t>(corrupted[bit / 8] ^ (1U << (bit % 8)));
            EXPECT_FALSE(proto::e2e::crc_ok(v.id, corrupted)) << "bit " << bit << " of id 0x" << std::hex << v.id;
        }
        for (unsigned bit = 0; bit < 11; ++bit) {
            EXPECT_FALSE(proto::e2e::crc_ok(v.id ^ (1U << bit), v.protected_frame)) << "id bit " << bit;
        }
    }
}

// A frame received under another message's identifier fails even with a valid
// payload CRC for its own identifier (the "data ID" argument for covering the id).
TEST(E2e, MisroutedFrameIsRejected)
{
    proto::SensorHeartbeat hb{};
    hb.uptime = 123456;
    const auto frame = proto::e2e::protect(hb, 3);
    EXPECT_TRUE(proto::e2e::crc_ok(proto::SensorHeartbeat::kId, frame));
    EXPECT_FALSE(proto::e2e::crc_ok(proto::ControllerHeartbeat::kId, frame));
}

TEST(E2e, CounterGeneratorWraps)
{
    proto::e2e::CounterGenerator gen;
    for (unsigned expected = 0; expected < 16; ++expected) {
        EXPECT_EQ(gen.next(), expected);
    }
    EXPECT_EQ(gen.next(), 0);
}

TEST(E2e, CounterTrackerPolicy)
{
    CounterTracker t;
    EXPECT_FALSE(t.has_last());
    EXPECT_EQ(t.check(3), CounterStatus::kOk);  // first frame always accepted
    EXPECT_EQ(t.check(4), CounterStatus::kOk);
    EXPECT_EQ(t.check(4), CounterStatus::kRepeated);
    EXPECT_EQ(t.check(6), CounterStatus::kSkipped);  // one lost
    EXPECT_EQ(t.lost_total(), 1U);
    EXPECT_EQ(t.check(15), CounterStatus::kOutOfSequence);  // delta 9: reference stays at 6
    EXPECT_EQ(t.last(), 6);
    EXPECT_EQ(t.check(7), CounterStatus::kOk);  // stream resumes after the late frame
    EXPECT_EQ(t.check(15), CounterStatus::kSkipped);  // delta 8: seven lost, still tolerated
    EXPECT_EQ(t.lost_total(), 8U);
    EXPECT_EQ(t.check(0), CounterStatus::kOk);  // wrap-around 15 -> 0
    EXPECT_EQ(t.check(15), CounterStatus::kOutOfSequence);  // went back by one (delta 15)
    EXPECT_EQ(t.last(), 0);
    t.reset();
    EXPECT_FALSE(t.has_last());
    EXPECT_EQ(t.check(11), CounterStatus::kOk);
}

// A single late (or replayed) frame must cost exactly one fault: the reference
// must not follow it backwards, or the next good frame would look like a skip.
TEST(E2e, LateFrameDoesNotProduceAPhantomLoss)
{
    CounterTracker t;
    EXPECT_EQ(t.check(5), CounterStatus::kOk);
    EXPECT_EQ(t.check(6), CounterStatus::kOk);
    EXPECT_EQ(t.check(7), CounterStatus::kOk);
    EXPECT_EQ(t.check(2), CounterStatus::kOutOfSequence);
    EXPECT_EQ(t.check(8), CounterStatus::kOk);
    EXPECT_EQ(t.check(9), CounterStatus::kOk);
    EXPECT_EQ(t.lost_total(), 0U);
}

TEST(E2e, CounterTrackerIgnoresUpperBits)
{
    CounterTracker t;
    t.check(0x15);  // 5
    EXPECT_EQ(t.check(0xF6), CounterStatus::kOk);  // 6
}

TEST(E2e, CounterStatusNames)
{
    EXPECT_STREQ(proto::e2e::to_string(CounterStatus::kOk), "ok");
    EXPECT_STREQ(proto::e2e::to_string(CounterStatus::kRepeated), "repeated");
    EXPECT_STREQ(proto::e2e::to_string(CounterStatus::kSkipped), "skipped");
    EXPECT_STREQ(proto::e2e::to_string(CounterStatus::kOutOfSequence), "out_of_sequence");
    EXPECT_STREQ(proto::e2e::to_string(static_cast<CounterStatus>(9)), "unknown");
}

TEST(E2e, WorksAtCompileTime)
{
    constexpr auto protected_command = [] {
        proto::ControllerCommand cmd{};
        cmd.torque_cmd = -100;
        cmd.mode = proto::ControllerMode::kSafeState;
        return proto::e2e::protect(cmd, 5);
    };
    constexpr auto frame = protected_command();
    static_assert(proto::e2e::crc_ok(proto::ControllerCommand::kId, frame), "constexpr protect/check");
    static_assert(proto::e2e::counter_of(frame) == 5, "constexpr counter");
    static_assert(proto::ControllerCommand::from_frame(frame).mode == proto::ControllerMode::kSafeState, "constexpr from_frame");
    EXPECT_EQ(protected_command(), frame) << "and the same code at run time";
}

}  // namespace

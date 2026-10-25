#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "generated/test_vectors.hpp"
#include "protocol/messages.hpp"

namespace {

// For every vector produced by cantools: unpack gives the expected raw values,
// pack gives back the exact bytes, and the pointer overload behaves.
template <class Msg, class Vectors>
void check_codec(const Vectors& vectors)
{
    for (const auto& v : vectors) {
        const Msg decoded = Msg::from_frame(v.bytes);
        EXPECT_TRUE(decoded == v.expected) << Msg::kName;
        EXPECT_FALSE(decoded != v.expected) << Msg::kName;
        EXPECT_EQ(v.expected.to_frame(), v.bytes) << Msg::kName;
        EXPECT_EQ(decoded.to_frame(), v.bytes) << Msg::kName;

        Msg from_buffer{};
        EXPECT_TRUE(Msg::from_buffer(v.bytes.data(), v.bytes.size(), from_buffer));
        EXPECT_TRUE(from_buffer == v.expected);
        EXPECT_FALSE(Msg::from_buffer(v.bytes.data(), Msg::kDlc - 1, from_buffer)) << "short buffer accepted";
        EXPECT_FALSE(Msg::from_buffer(nullptr, Msg::kDlc, from_buffer));

        // A longer buffer is accepted and only the first kDlc bytes are read
        // (the receiver checks the DLC before decoding; this pins the policy).
        std::array<std::uint8_t, 16> longer{};
        for (std::size_t i = 0; i < v.bytes.size(); ++i) {
            longer[i] = v.bytes[i];
        }
        longer[Msg::kDlc] = 0xFF;
        EXPECT_TRUE(Msg::from_buffer(longer.data(), longer.size(), from_buffer));
        EXPECT_TRUE(from_buffer == v.expected);
    }
    // Two vectors of the same message type differ (operator!= is not trivially false).
    if (vectors.size() >= 2) {
        EXPECT_TRUE(vectors[0].expected != vectors[1].expected) << Msg::kName;
    }
}

// Physical accessor against cantools' scaled decode, and back to the same raw.
#define CHECK_PHYS(msg, vec, field, idx)                                                       \
    do {                                                                                      \
        const double scale = static_cast<double>(proto::msg::kFields[idx].scale);              \
        EXPECT_NEAR(static_cast<double>((vec).expected.field##_phys()), (vec).phys[idx],       \
                    scale * 0.01)                                                             \
            << #msg "." #field;                                                               \
        proto::msg copy = (vec).expected;                                                     \
        copy.set_##field##_phys(static_cast<float>((vec).phys[idx]));                          \
        EXPECT_EQ(copy.field, (vec).expected.field) << #msg "." #field " set_phys round trip"; \
    } while (0)

TEST(Messages, ControllerCommand)
{
    check_codec<proto::ControllerCommand>(vectors::kControllerCommand);
    using Idx = vectors::ControllerCommandIdx;
    for (const auto& v : vectors::kControllerCommand) {
        CHECK_PHYS(ControllerCommand, v, torque_cmd, Idx::torque_cmd);
        CHECK_PHYS(ControllerCommand, v, steering_cmd, Idx::steering_cmd);
        CHECK_PHYS(ControllerCommand, v, speed_limit, Idx::speed_limit);
    }
}

TEST(Messages, SensorData)
{
    check_codec<proto::SensorData>(vectors::kSensorData);
    using Idx = vectors::SensorDataIdx;
    for (const auto& v : vectors::kSensorData) {
        CHECK_PHYS(SensorData, v, speed, Idx::speed);
        CHECK_PHYS(SensorData, v, yaw_rate, Idx::yaw_rate);
        CHECK_PHYS(SensorData, v, accel_x, Idx::accel_x);
    }
}

TEST(Messages, BatteryStatus)
{
    check_codec<proto::BatteryStatus>(vectors::kBatteryStatus);
    using Idx = vectors::BatteryStatusIdx;
    for (const auto& v : vectors::kBatteryStatus) {
        CHECK_PHYS(BatteryStatus, v, pack_voltage, Idx::pack_voltage);
        CHECK_PHYS(BatteryStatus, v, pack_current, Idx::pack_current);
        CHECK_PHYS(BatteryStatus, v, state_of_charge, Idx::state_of_charge);
        CHECK_PHYS(BatteryStatus, v, temperature, Idx::temperature);
    }
}

TEST(Messages, Heartbeats)
{
    check_codec<proto::SensorHeartbeat>(vectors::kSensorHeartbeat);
    check_codec<proto::ControllerHeartbeat>(vectors::kControllerHeartbeat);
}

TEST(Messages, EchoAndTestControl)
{
    check_codec<proto::EchoRequest>(vectors::kEchoRequest);
    check_codec<proto::SensorEchoResponse>(vectors::kSensorEchoResponse);
    check_codec<proto::ControllerEchoResponse>(vectors::kControllerEchoResponse);
    check_codec<proto::TestControl>(vectors::kTestControl);
    EXPECT_EQ(proto::TestControl::kMagic, 0xC0DE);
    // One transmitter per identifier: the two echo responses have distinct IDs
    // and the same layout, so the host can decode both with one code path.
    EXPECT_EQ(proto::SensorEchoResponse::kId, 0x301U);
    EXPECT_EQ(proto::ControllerEchoResponse::kId, 0x302U);
    EXPECT_EQ(proto::SensorEchoResponse::kDlc, proto::ControllerEchoResponse::kDlc);
}

TEST(Messages, PhysicalSettersSaturate)
{
    proto::SensorData m{};
    m.set_speed_phys(1.0e6F);
    EXPECT_EQ(m.speed, 65535);
    m.set_speed_phys(-5.0F);
    EXPECT_EQ(m.speed, 0);
    m.set_yaw_rate_phys(-1.0e6F);
    EXPECT_EQ(m.yaw_rate, -32768);
    m.set_yaw_rate_phys(1.0e6F);
    EXPECT_EQ(m.yaw_rate, 32767);

    proto::BatteryStatus b{};
    b.set_state_of_charge_phys(150.0F);  // DBC range is 0..100 %, raw 0..200
    EXPECT_EQ(b.state_of_charge, 200);
    b.set_temperature_phys(-100.0F);
    EXPECT_EQ(b.temperature, 0);  // -40 degC
    EXPECT_FLOAT_EQ(b.temperature_phys(), -40.0F);
}

// The network definition the firmware is built against (the requirements).
TEST(Messages, NetworkDefinition)
{
    EXPECT_EQ(proto::SensorData::kCycleMs, 10);
    EXPECT_EQ(proto::BatteryStatus::kCycleMs, 100);
    EXPECT_EQ(proto::ControllerCommand::kCycleMs, 20);
    EXPECT_EQ(proto::SensorHeartbeat::kCycleMs, 100);
    EXPECT_EQ(proto::ControllerHeartbeat::kCycleMs, 100);
    EXPECT_EQ(proto::EchoRequest::kCycleMs, 0);
    EXPECT_EQ(proto::SensorEchoResponse::kCycleMs, 0);
    EXPECT_EQ(proto::ControllerEchoResponse::kCycleMs, 0);
    EXPECT_EQ(proto::TestControl::kCycleMs, 0);

    EXPECT_TRUE(proto::SensorData::kSafetyRelevant);
    EXPECT_TRUE(proto::BatteryStatus::kSafetyRelevant);
    EXPECT_TRUE(proto::ControllerCommand::kSafetyRelevant);
    EXPECT_TRUE(proto::SensorHeartbeat::kSafetyRelevant);
    EXPECT_TRUE(proto::ControllerHeartbeat::kSafetyRelevant);
    EXPECT_FALSE(proto::EchoRequest::kSafetyRelevant);
    EXPECT_FALSE(proto::SensorEchoResponse::kSafetyRelevant);
    EXPECT_FALSE(proto::ControllerEchoResponse::kSafetyRelevant);
    EXPECT_FALSE(proto::TestControl::kSafetyRelevant);

    EXPECT_TRUE(proto::TestControl::kTestOnly);
    EXPECT_FALSE(proto::SensorData::kTestOnly);

    // The command is the highest-priority frame on the bus, the test message the lowest.
    EXPECT_LT(proto::ControllerCommand::kId, proto::SensorData::kId);
    EXPECT_EQ(proto::TestControl::kId, 0x7E0U);
}

TEST(Messages, TableIsConsistent)
{
    std::set<std::uint32_t> ids;
    std::uint32_t previous = 0;
    bool first = true;
    for (const auto& info : proto::kMessages) {
        EXPECT_TRUE(ids.insert(info.id).second) << "duplicate id";
        EXPECT_LE(info.id, 0x7FFU);
        EXPECT_LE(info.dlc, 8U);
        EXPECT_GE(info.dlc, 1U);
        if (!first) {
            EXPECT_GT(info.id, previous) << "table must be ordered by id";
        }
        first = false;
        previous = info.id;
        EXPECT_EQ(proto::find_message(info.id), &info);
    }
    EXPECT_EQ(proto::kMessages.size(), 9U);
    EXPECT_EQ(proto::find_message(0x7FF), nullptr);
    EXPECT_STREQ(proto::find_message(proto::SensorData::kId)->name, "SENSOR_DATA");
    static_assert(proto::find_message(proto::SensorData::kId)->dlc == 8, "constexpr lookup");
}

// Every raw value the signal can carry on the wire maps to its DBC label, or to
// "UNKNOWN" when the value table has no entry (a corrupted or future value).
template <class EnumInfo>
void check_enum_over_full_range()
{
    using E = typename EnumInfo::Type;
    for (std::uint32_t raw = 0; raw < (1U << EnumInfo::kBits); ++raw) {
        const char* expected = "UNKNOWN";
        for (const auto& n : EnumInfo::kNames) {
            if (static_cast<std::uint32_t>(n.value) == raw) {
                expected = n.label;
            }
        }
        EXPECT_STREQ(proto::to_string(static_cast<E>(raw)), expected) << "raw " << raw;
    }
}

TEST(Messages, EnumToString)
{
    check_enum_over_full_range<vectors::ControllerModeEnum>();
    check_enum_over_full_range<vectors::SensorStatusEnum>();
    check_enum_over_full_range<vectors::NodeStateEnum>();
    check_enum_over_full_range<vectors::CanStateEnum>();
    check_enum_over_full_range<vectors::NodeEnum>();
    check_enum_over_full_range<vectors::TestCommandEnum>();
    EXPECT_STREQ(proto::to_string(proto::NodeState::kSafeState), "SAFE_STATE");
}

// An out-of-table value survives a decode unchanged; nothing "corrects" it.
TEST(Messages, OutOfTableEnumValuesAreKept)
{
    proto::SensorData m{};
    m.sensor_status = static_cast<proto::SensorStatus>(13);
    const auto back = proto::SensorData::from_frame(m.to_frame());
    EXPECT_EQ(static_cast<unsigned>(back.sensor_status), 13U);
    EXPECT_STREQ(proto::to_string(back.sensor_status), "UNKNOWN");
}

TEST(Messages, FieldInfoDescribesTheLayout)
{
    const auto& fields = proto::SensorData::kFields;
    EXPECT_STREQ(fields[0].name, "crc");
    EXPECT_EQ(fields[0].start_bit, 0);
    EXPECT_EQ(fields[0].length, 8);
    EXPECT_STREQ(fields[1].name, "counter");
    EXPECT_EQ(fields[1].start_bit, 8);
    EXPECT_EQ(fields[1].length, 4);
    EXPECT_STREQ(fields[3].name, "speed");
    EXPECT_FLOAT_EQ(fields[3].scale, 0.01F);
    EXPECT_STREQ(fields[3].unit, "m/s");
    EXPECT_TRUE(fields[4].is_signed);
    EXPECT_EQ(fields[4].raw_min, -32768);
}

TEST(Messages, RoundTripAtCompileTime)
{
    constexpr proto::BatteryStatus original = [] {
        proto::BatteryStatus b{};
        b.pack_voltage = 4800;
        b.pack_current = -125;
        b.state_of_charge = 177;
        b.temperature = 65;
        b.low_voltage = 1;
        return b;
    }();
    constexpr auto bytes = original.to_frame();
    static_assert(proto::BatteryStatus::from_frame(bytes) == original, "to_frame/from_frame must be an identity at compile time");
}

}  // namespace

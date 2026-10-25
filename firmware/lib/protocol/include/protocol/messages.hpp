// GENERATED FILE, do not edit.
// Source: can/network.dbc  (sha256 04c2dbe6e803b397)
// Generator: can/gen_codec.py  (regenerate with `python3 can/gen_codec.py`)
// Network version: 'can-ecu-sim network 1.0'
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol/codec_util.hpp"

namespace proto {

enum class ControllerMode : std::uint8_t {
    kInit = 0,
    kNormal = 1,
    kDegraded = 2,
    kSafeState = 3,
};

constexpr const char* to_string(ControllerMode value)
{
    switch (value) {
    case ControllerMode::kInit: return "INIT";
    case ControllerMode::kNormal: return "NORMAL";
    case ControllerMode::kDegraded: return "DEGRADED";
    case ControllerMode::kSafeState: return "SAFE_STATE";
    }
    return "UNKNOWN";
}

enum class SensorStatus : std::uint8_t {
    kOk = 0,
    kDegraded = 1,
    kFault = 2,
};

constexpr const char* to_string(SensorStatus value)
{
    switch (value) {
    case SensorStatus::kOk: return "OK";
    case SensorStatus::kDegraded: return "DEGRADED";
    case SensorStatus::kFault: return "FAULT";
    }
    return "UNKNOWN";
}

enum class NodeState : std::uint8_t {
    kBoot = 0,
    kRunning = 1,
    kDegraded = 2,
    kSafeState = 3,
    kFault = 4,
};

constexpr const char* to_string(NodeState value)
{
    switch (value) {
    case NodeState::kBoot: return "BOOT";
    case NodeState::kRunning: return "RUNNING";
    case NodeState::kDegraded: return "DEGRADED";
    case NodeState::kSafeState: return "SAFE_STATE";
    case NodeState::kFault: return "FAULT";
    }
    return "UNKNOWN";
}

enum class CanState : std::uint8_t {
    kErrorActive = 0,
    kErrorPassive = 1,
    kBusOff = 2,
    kStopped = 3,
};

constexpr const char* to_string(CanState value)
{
    switch (value) {
    case CanState::kErrorActive: return "ERROR_ACTIVE";
    case CanState::kErrorPassive: return "ERROR_PASSIVE";
    case CanState::kBusOff: return "BUS_OFF";
    case CanState::kStopped: return "STOPPED";
    }
    return "UNKNOWN";
}

enum class Node : std::uint8_t {
    kAll = 0,
    kSensorEcu = 1,
    kControllerEcu = 2,
};

constexpr const char* to_string(Node value)
{
    switch (value) {
    case Node::kAll: return "ALL";
    case Node::kSensorEcu: return "SENSOR_ECU";
    case Node::kControllerEcu: return "CONTROLLER_ECU";
    }
    return "UNKNOWN";
}

enum class TestCommand : std::uint8_t {
    kNop = 0,
    kResetFaults = 1,
    kInjectCanError = 2,
    kForceBusOff = 3,
    kSuppressHeartbeat = 4,
    kCorruptCrc = 5,
    kSkipCounter = 6,
    kFreezeCounter = 7,
    kSuppressMessage = 8,
};

constexpr const char* to_string(TestCommand value)
{
    switch (value) {
    case TestCommand::kNop: return "NOP";
    case TestCommand::kResetFaults: return "RESET_FAULTS";
    case TestCommand::kInjectCanError: return "INJECT_CAN_ERROR";
    case TestCommand::kForceBusOff: return "FORCE_BUS_OFF";
    case TestCommand::kSuppressHeartbeat: return "SUPPRESS_HEARTBEAT";
    case TestCommand::kCorruptCrc: return "CORRUPT_CRC";
    case TestCommand::kSkipCounter: return "SKIP_COUNTER";
    case TestCommand::kFreezeCounter: return "FREEZE_COUNTER";
    case TestCommand::kSuppressMessage: return "SUPPRESS_MESSAGE";
    }
    return "UNKNOWN";
}

/// Static description of one signal, for diagnostics and generic tooling.
struct FieldInfo {
    const char* name;
    std::uint8_t start_bit;
    std::uint8_t length;
    bool is_signed;
    float scale;
    float offset;
    std::int64_t raw_min;
    std::int64_t raw_max;
    const char* unit;
};

/// Static description of one message.
struct MessageInfo {
    std::uint32_t id;
    std::uint8_t dlc;
    std::uint16_t cycle_ms;  ///< 0 for event-driven messages
    bool safety_relevant;    ///< carries CRC + rolling counter
    bool test_only;          ///< accepted only by test builds
    const char* name;
};

/// Actuator command from the controller, 20 ms. Highest priority on the bus. mode mirrors the controller state machine so tests can observe safe-state entry on the bus.
/// Senders: CONTROLLER_ECU
struct ControllerCommand {
    static constexpr std::uint32_t kId = 0x080U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 20;
    static constexpr bool kSafetyRelevant = true;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "CONTROLLER_COMMAND";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t crc{};  ///< bits 0..7, raw 0..255
    std::uint8_t counter{};  ///< bits 8..11, raw 0..15
    ControllerMode mode{};  ///< bits 12..15, raw 0..15. Controller mode, see value table.
    std::int16_t torque_cmd{};  ///< bits 16..31, phys = raw * 0.01 + 0 [Nm], raw -32768..32767
    std::int16_t steering_cmd{};  ///< bits 32..47, phys = raw * 0.01 + 0 [deg], raw -32768..32767
    std::uint8_t actuators_enabled{};  ///< bits 48..48, raw 0..1
    std::uint8_t brake_hold{};  ///< bits 49..49, raw 0..1
    std::uint8_t fault_latched{};  ///< bits 50..50, raw 0..1
    std::uint8_t speed_limit{};  ///< bits 56..63, phys = raw * 0.1 + 0 [m/s], raw 0..255. Speed ceiling the controller currently enforces (derated on low battery).

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    constexpr float torque_cmd_phys() const
    {
        return codec::raw_to_phys(torque_cmd, 0.01F, 0.0F);
    }
    constexpr void set_torque_cmd_phys(float value)
    {
        torque_cmd = static_cast<std::int16_t>(
            codec::phys_to_raw(value, 0.01F, 0.0F, -32768, 32767));
    }
    constexpr float steering_cmd_phys() const
    {
        return codec::raw_to_phys(steering_cmd, 0.01F, 0.0F);
    }
    constexpr void set_steering_cmd_phys(float value)
    {
        steering_cmd = static_cast<std::int16_t>(
            codec::phys_to_raw(value, 0.01F, 0.0F, -32768, 32767));
    }
    constexpr float speed_limit_phys() const
    {
        return codec::raw_to_phys(speed_limit, 0.1F, 0.0F);
    }
    constexpr void set_speed_limit_phys(float value)
    {
        speed_limit = static_cast<std::uint8_t>(
            codec::phys_to_raw(value, 0.1F, 0.0F, 0, 255));
    }

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(crc));
        codec::put(d, 8, 4, static_cast<std::uint32_t>(counter));
        codec::put(d, 12, 4, static_cast<std::uint32_t>(mode));
        codec::put(d, 16, 16, static_cast<std::uint32_t>(torque_cmd));
        codec::put(d, 32, 16, static_cast<std::uint32_t>(steering_cmd));
        codec::put(d, 48, 1, static_cast<std::uint32_t>(actuators_enabled));
        codec::put(d, 49, 1, static_cast<std::uint32_t>(brake_hold));
        codec::put(d, 50, 1, static_cast<std::uint32_t>(fault_latched));
        codec::put(d, 56, 8, static_cast<std::uint32_t>(speed_limit));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr ControllerCommand from_frame(const Frame& d)
    {
        ControllerCommand m{};
        m.crc = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.counter = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 4));
        m.mode = static_cast<ControllerMode>(codec::get_unsigned(d, 12, 4));
        m.torque_cmd = static_cast<std::int16_t>(codec::get_signed(d, 16, 16));
        m.steering_cmd = static_cast<std::int16_t>(codec::get_signed(d, 32, 16));
        m.actuators_enabled = static_cast<std::uint8_t>(codec::get_unsigned(d, 48, 1));
        m.brake_hold = static_cast<std::uint8_t>(codec::get_unsigned(d, 49, 1));
        m.fault_latched = static_cast<std::uint8_t>(codec::get_unsigned(d, 50, 1));
        m.speed_limit = static_cast<std::uint8_t>(codec::get_unsigned(d, 56, 8));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, ControllerCommand& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 9> kFields = {{
        {"crc", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"counter", 8, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"mode", 12, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"torque_cmd", 16, 16, true, 0.01F, 0.0F, -32768, 32767, "Nm"},
        {"steering_cmd", 32, 16, true, 0.01F, 0.0F, -32768, 32767, "deg"},
        {"actuators_enabled", 48, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"brake_hold", 49, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"fault_latched", 50, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"speed_limit", 56, 8, false, 0.1F, 0.0F, 0, 255, "m/s"},
    }};
};

constexpr bool operator==(const ControllerCommand& a, const ControllerCommand& b)
{
    return a.crc == b.crc
        && a.counter == b.counter
        && a.mode == b.mode
        && a.torque_cmd == b.torque_cmd
        && a.steering_cmd == b.steering_cmd
        && a.actuators_enabled == b.actuators_enabled
        && a.brake_hold == b.brake_hold
        && a.fault_latched == b.fault_latched
        && a.speed_limit == b.speed_limit;
}
constexpr bool operator!=(const ControllerCommand& a, const ControllerCommand& b)
{
    return !(a == b);
}

/// Simulated vehicle sensor sample, 10 ms.
/// Senders: SENSOR_ECU
struct SensorData {
    static constexpr std::uint32_t kId = 0x100U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 10;
    static constexpr bool kSafetyRelevant = true;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "SENSOR_DATA";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t crc{};  ///< bits 0..7, raw 0..255
    std::uint8_t counter{};  ///< bits 8..11, raw 0..15
    SensorStatus sensor_status{};  ///< bits 12..15, raw 0..15. Sensor health, see value table.
    std::uint16_t speed{};  ///< bits 16..31, phys = raw * 0.01 + 0 [m/s], raw 0..65535
    std::int16_t yaw_rate{};  ///< bits 32..47, phys = raw * 0.1 + 0 [deg/s], raw -32768..32767
    std::int16_t accel_x{};  ///< bits 48..63, phys = raw * 0.01 + 0 [m/s^2], raw -32768..32767

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    constexpr float speed_phys() const
    {
        return codec::raw_to_phys(speed, 0.01F, 0.0F);
    }
    constexpr void set_speed_phys(float value)
    {
        speed = static_cast<std::uint16_t>(
            codec::phys_to_raw(value, 0.01F, 0.0F, 0, 65535));
    }
    constexpr float yaw_rate_phys() const
    {
        return codec::raw_to_phys(yaw_rate, 0.1F, 0.0F);
    }
    constexpr void set_yaw_rate_phys(float value)
    {
        yaw_rate = static_cast<std::int16_t>(
            codec::phys_to_raw(value, 0.1F, 0.0F, -32768, 32767));
    }
    constexpr float accel_x_phys() const
    {
        return codec::raw_to_phys(accel_x, 0.01F, 0.0F);
    }
    constexpr void set_accel_x_phys(float value)
    {
        accel_x = static_cast<std::int16_t>(
            codec::phys_to_raw(value, 0.01F, 0.0F, -32768, 32767));
    }

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(crc));
        codec::put(d, 8, 4, static_cast<std::uint32_t>(counter));
        codec::put(d, 12, 4, static_cast<std::uint32_t>(sensor_status));
        codec::put(d, 16, 16, static_cast<std::uint32_t>(speed));
        codec::put(d, 32, 16, static_cast<std::uint32_t>(yaw_rate));
        codec::put(d, 48, 16, static_cast<std::uint32_t>(accel_x));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr SensorData from_frame(const Frame& d)
    {
        SensorData m{};
        m.crc = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.counter = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 4));
        m.sensor_status = static_cast<SensorStatus>(codec::get_unsigned(d, 12, 4));
        m.speed = static_cast<std::uint16_t>(codec::get_unsigned(d, 16, 16));
        m.yaw_rate = static_cast<std::int16_t>(codec::get_signed(d, 32, 16));
        m.accel_x = static_cast<std::int16_t>(codec::get_signed(d, 48, 16));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, SensorData& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 6> kFields = {{
        {"crc", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"counter", 8, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"sensor_status", 12, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"speed", 16, 16, false, 0.01F, 0.0F, 0, 65535, "m/s"},
        {"yaw_rate", 32, 16, true, 0.1F, 0.0F, -32768, 32767, "deg/s"},
        {"accel_x", 48, 16, true, 0.01F, 0.0F, -32768, 32767, "m/s^2"},
    }};
};

constexpr bool operator==(const SensorData& a, const SensorData& b)
{
    return a.crc == b.crc
        && a.counter == b.counter
        && a.sensor_status == b.sensor_status
        && a.speed == b.speed
        && a.yaw_rate == b.yaw_rate
        && a.accel_x == b.accel_x;
}
constexpr bool operator!=(const SensorData& a, const SensorData& b)
{
    return !(a == b);
}

/// Battery pack status, 100 ms.
/// Senders: SENSOR_ECU
struct BatteryStatus {
    static constexpr std::uint32_t kId = 0x110U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 100;
    static constexpr bool kSafetyRelevant = true;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "BATTERY_STATUS";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t crc{};  ///< bits 0..7, raw 0..255
    std::uint8_t counter{};  ///< bits 8..11, raw 0..15
    std::uint8_t charging{};  ///< bits 12..12, raw 0..1
    std::uint8_t low_voltage{};  ///< bits 13..13, raw 0..1
    std::uint8_t over_temperature{};  ///< bits 14..14, raw 0..1
    std::uint16_t pack_voltage{};  ///< bits 16..31, phys = raw * 0.01 + 0 [V], raw 0..65535
    std::int16_t pack_current{};  ///< bits 32..47, phys = raw * 0.1 + 0 [A], raw -32768..32767
    std::uint8_t state_of_charge{};  ///< bits 48..55, phys = raw * 0.5 + 0 [%], raw 0..200
    std::uint8_t temperature{};  ///< bits 56..63, phys = raw * 1 + -40 [degC], raw 0..255

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    constexpr float pack_voltage_phys() const
    {
        return codec::raw_to_phys(pack_voltage, 0.01F, 0.0F);
    }
    constexpr void set_pack_voltage_phys(float value)
    {
        pack_voltage = static_cast<std::uint16_t>(
            codec::phys_to_raw(value, 0.01F, 0.0F, 0, 65535));
    }
    constexpr float pack_current_phys() const
    {
        return codec::raw_to_phys(pack_current, 0.1F, 0.0F);
    }
    constexpr void set_pack_current_phys(float value)
    {
        pack_current = static_cast<std::int16_t>(
            codec::phys_to_raw(value, 0.1F, 0.0F, -32768, 32767));
    }
    constexpr float state_of_charge_phys() const
    {
        return codec::raw_to_phys(state_of_charge, 0.5F, 0.0F);
    }
    constexpr void set_state_of_charge_phys(float value)
    {
        state_of_charge = static_cast<std::uint8_t>(
            codec::phys_to_raw(value, 0.5F, 0.0F, 0, 200));
    }
    constexpr float temperature_phys() const
    {
        return codec::raw_to_phys(temperature, 1.0F, -40.0F);
    }
    constexpr void set_temperature_phys(float value)
    {
        temperature = static_cast<std::uint8_t>(
            codec::phys_to_raw(value, 1.0F, -40.0F, 0, 255));
    }

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(crc));
        codec::put(d, 8, 4, static_cast<std::uint32_t>(counter));
        codec::put(d, 12, 1, static_cast<std::uint32_t>(charging));
        codec::put(d, 13, 1, static_cast<std::uint32_t>(low_voltage));
        codec::put(d, 14, 1, static_cast<std::uint32_t>(over_temperature));
        codec::put(d, 16, 16, static_cast<std::uint32_t>(pack_voltage));
        codec::put(d, 32, 16, static_cast<std::uint32_t>(pack_current));
        codec::put(d, 48, 8, static_cast<std::uint32_t>(state_of_charge));
        codec::put(d, 56, 8, static_cast<std::uint32_t>(temperature));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr BatteryStatus from_frame(const Frame& d)
    {
        BatteryStatus m{};
        m.crc = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.counter = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 4));
        m.charging = static_cast<std::uint8_t>(codec::get_unsigned(d, 12, 1));
        m.low_voltage = static_cast<std::uint8_t>(codec::get_unsigned(d, 13, 1));
        m.over_temperature = static_cast<std::uint8_t>(codec::get_unsigned(d, 14, 1));
        m.pack_voltage = static_cast<std::uint16_t>(codec::get_unsigned(d, 16, 16));
        m.pack_current = static_cast<std::int16_t>(codec::get_signed(d, 32, 16));
        m.state_of_charge = static_cast<std::uint8_t>(codec::get_unsigned(d, 48, 8));
        m.temperature = static_cast<std::uint8_t>(codec::get_unsigned(d, 56, 8));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, BatteryStatus& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 9> kFields = {{
        {"crc", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"counter", 8, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"charging", 12, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"low_voltage", 13, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"over_temperature", 14, 1, false, 1.0F, 0.0F, 0, 1, ""},
        {"pack_voltage", 16, 16, false, 0.01F, 0.0F, 0, 65535, "V"},
        {"pack_current", 32, 16, true, 0.1F, 0.0F, -32768, 32767, "A"},
        {"state_of_charge", 48, 8, false, 0.5F, 0.0F, 0, 200, "%"},
        {"temperature", 56, 8, false, 1.0F, -40.0F, 0, 255, "degC"},
    }};
};

constexpr bool operator==(const BatteryStatus& a, const BatteryStatus& b)
{
    return a.crc == b.crc
        && a.counter == b.counter
        && a.charging == b.charging
        && a.low_voltage == b.low_voltage
        && a.over_temperature == b.over_temperature
        && a.pack_voltage == b.pack_voltage
        && a.pack_current == b.pack_current
        && a.state_of_charge == b.state_of_charge
        && a.temperature == b.temperature;
}
constexpr bool operator!=(const BatteryStatus& a, const BatteryStatus& b)
{
    return !(a == b);
}

/// Sensor ECU alive message, 100 ms. Loss drives the controller to its safe state.
/// Senders: SENSOR_ECU
struct SensorHeartbeat {
    static constexpr std::uint32_t kId = 0x200U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 100;
    static constexpr bool kSafetyRelevant = true;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "SENSOR_HEARTBEAT";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t crc{};  ///< bits 0..7, raw 0..255
    std::uint8_t counter{};  ///< bits 8..11, raw 0..15
    NodeState node_state{};  ///< bits 12..15, raw 0..15
    std::uint32_t uptime{};  ///< bits 16..47 [ms], raw 0..4294967295
    CanState can_state{};  ///< bits 48..49, raw 0..3. Local CAN controller state as reported by the driver, see value table. The driver's error-warning level (an error counter at or above 96) is still error-active per ISO 11898-1 and is reported as ERROR_ACTIVE; the counters, not the state, show the warning level.
    std::uint8_t bus_off_count{};  ///< bits 50..55, raw 0..63
    std::uint8_t error_flags{};  ///< bits 56..63, raw 0..255. Latched diagnostic bits: bit 0 crc_fault_seen, bit 1 counter_fault_seen, bit 2 rx_timeout_seen, bit 3 bus_off_seen, bit 4 watchdog_warning, bit 5 test_hook_active, bits 6-7 reserved.

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(crc));
        codec::put(d, 8, 4, static_cast<std::uint32_t>(counter));
        codec::put(d, 12, 4, static_cast<std::uint32_t>(node_state));
        codec::put(d, 16, 32, static_cast<std::uint32_t>(uptime));
        codec::put(d, 48, 2, static_cast<std::uint32_t>(can_state));
        codec::put(d, 50, 6, static_cast<std::uint32_t>(bus_off_count));
        codec::put(d, 56, 8, static_cast<std::uint32_t>(error_flags));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr SensorHeartbeat from_frame(const Frame& d)
    {
        SensorHeartbeat m{};
        m.crc = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.counter = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 4));
        m.node_state = static_cast<NodeState>(codec::get_unsigned(d, 12, 4));
        m.uptime = static_cast<std::uint32_t>(codec::get_unsigned(d, 16, 32));
        m.can_state = static_cast<CanState>(codec::get_unsigned(d, 48, 2));
        m.bus_off_count = static_cast<std::uint8_t>(codec::get_unsigned(d, 50, 6));
        m.error_flags = static_cast<std::uint8_t>(codec::get_unsigned(d, 56, 8));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, SensorHeartbeat& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 7> kFields = {{
        {"crc", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"counter", 8, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"node_state", 12, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"uptime", 16, 32, false, 1.0F, 0.0F, 0, 4294967295, "ms"},
        {"can_state", 48, 2, false, 1.0F, 0.0F, 0, 3, ""},
        {"bus_off_count", 50, 6, false, 1.0F, 0.0F, 0, 63, ""},
        {"error_flags", 56, 8, false, 1.0F, 0.0F, 0, 255, ""},
    }};
};

constexpr bool operator==(const SensorHeartbeat& a, const SensorHeartbeat& b)
{
    return a.crc == b.crc
        && a.counter == b.counter
        && a.node_state == b.node_state
        && a.uptime == b.uptime
        && a.can_state == b.can_state
        && a.bus_off_count == b.bus_off_count
        && a.error_flags == b.error_flags;
}
constexpr bool operator!=(const SensorHeartbeat& a, const SensorHeartbeat& b)
{
    return !(a == b);
}

/// Controller ECU alive message, 100 ms.
/// Senders: CONTROLLER_ECU
struct ControllerHeartbeat {
    static constexpr std::uint32_t kId = 0x201U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 100;
    static constexpr bool kSafetyRelevant = true;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "CONTROLLER_HEARTBEAT";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t crc{};  ///< bits 0..7, raw 0..255
    std::uint8_t counter{};  ///< bits 8..11, raw 0..15
    NodeState node_state{};  ///< bits 12..15, raw 0..15
    std::uint32_t uptime{};  ///< bits 16..47 [ms], raw 0..4294967295
    CanState can_state{};  ///< bits 48..49, raw 0..3. Local CAN controller state as reported by the driver, see value table. The driver's error-warning level is reported as ERROR_ACTIVE, see SENSOR_HEARTBEAT.can_state.
    std::uint8_t bus_off_count{};  ///< bits 50..55, raw 0..63
    std::uint8_t error_flags{};  ///< bits 56..63, raw 0..255. Latched diagnostic bits: bit 0 crc_fault_seen, bit 1 counter_fault_seen, bit 2 rx_timeout_seen, bit 3 bus_off_seen, bit 4 watchdog_warning, bit 5 test_hook_active, bits 6-7 reserved.

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(crc));
        codec::put(d, 8, 4, static_cast<std::uint32_t>(counter));
        codec::put(d, 12, 4, static_cast<std::uint32_t>(node_state));
        codec::put(d, 16, 32, static_cast<std::uint32_t>(uptime));
        codec::put(d, 48, 2, static_cast<std::uint32_t>(can_state));
        codec::put(d, 50, 6, static_cast<std::uint32_t>(bus_off_count));
        codec::put(d, 56, 8, static_cast<std::uint32_t>(error_flags));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr ControllerHeartbeat from_frame(const Frame& d)
    {
        ControllerHeartbeat m{};
        m.crc = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.counter = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 4));
        m.node_state = static_cast<NodeState>(codec::get_unsigned(d, 12, 4));
        m.uptime = static_cast<std::uint32_t>(codec::get_unsigned(d, 16, 32));
        m.can_state = static_cast<CanState>(codec::get_unsigned(d, 48, 2));
        m.bus_off_count = static_cast<std::uint8_t>(codec::get_unsigned(d, 50, 6));
        m.error_flags = static_cast<std::uint8_t>(codec::get_unsigned(d, 56, 8));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, ControllerHeartbeat& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 7> kFields = {{
        {"crc", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"counter", 8, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"node_state", 12, 4, false, 1.0F, 0.0F, 0, 15, ""},
        {"uptime", 16, 32, false, 1.0F, 0.0F, 0, 4294967295, "ms"},
        {"can_state", 48, 2, false, 1.0F, 0.0F, 0, 3, ""},
        {"bus_off_count", 50, 6, false, 1.0F, 0.0F, 0, 63, ""},
        {"error_flags", 56, 8, false, 1.0F, 0.0F, 0, 255, ""},
    }};
};

constexpr bool operator==(const ControllerHeartbeat& a, const ControllerHeartbeat& b)
{
    return a.crc == b.crc
        && a.counter == b.counter
        && a.node_state == b.node_state
        && a.uptime == b.uptime
        && a.can_state == b.can_state
        && a.bus_off_count == b.bus_off_count
        && a.error_flags == b.error_flags;
}
constexpr bool operator!=(const ControllerHeartbeat& a, const ControllerHeartbeat& b)
{
    return !(a == b);
}

/// Round-trip latency probe from the host. The addressed node (or every node for target_node 0) answers with its own echo response (SENSOR_ECHO_RESPONSE or CONTROLLER_ECHO_RESPONSE) carrying the same seq and payload. Each responder has its own identifier because two nodes transmitting one identifier would collide in the data field on a real bus. Not safety relevant, no CRC.
/// Senders: TEST_HOST
struct EchoRequest {
    static constexpr std::uint32_t kId = 0x300U;
    static constexpr std::size_t kDlc = 6;
    static constexpr std::uint16_t kCycleMs = 0;
    static constexpr bool kSafetyRelevant = false;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "ECHO_REQUEST";
    using Frame = std::array<std::uint8_t, kDlc>;

    Node target_node{};  ///< bits 0..7, raw 0..255
    std::uint8_t seq{};  ///< bits 8..15, raw 0..255
    std::uint32_t payload{};  ///< bits 16..47, raw 0..4294967295

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(target_node));
        codec::put(d, 8, 8, static_cast<std::uint32_t>(seq));
        codec::put(d, 16, 32, static_cast<std::uint32_t>(payload));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr EchoRequest from_frame(const Frame& d)
    {
        EchoRequest m{};
        m.target_node = static_cast<Node>(codec::get_unsigned(d, 0, 8));
        m.seq = static_cast<std::uint8_t>(codec::get_unsigned(d, 8, 8));
        m.payload = static_cast<std::uint32_t>(codec::get_unsigned(d, 16, 32));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, EchoRequest& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 3> kFields = {{
        {"target_node", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"seq", 8, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"payload", 16, 32, false, 1.0F, 0.0F, 0, 4294967295, ""},
    }};
};

constexpr bool operator==(const EchoRequest& a, const EchoRequest& b)
{
    return a.target_node == b.target_node
        && a.seq == b.seq
        && a.payload == b.payload;
}
constexpr bool operator!=(const EchoRequest& a, const EchoRequest& b)
{
    return !(a == b);
}

/// Sensor ECU's answer to ECHO_REQUEST. turnaround is the responder's own receive-to-transmit time, which lets the host separate ECU processing time from bus and OS latency.
/// Senders: SENSOR_ECU
struct SensorEchoResponse {
    static constexpr std::uint32_t kId = 0x301U;
    static constexpr std::size_t kDlc = 7;
    static constexpr std::uint16_t kCycleMs = 0;
    static constexpr bool kSafetyRelevant = false;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "SENSOR_ECHO_RESPONSE";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t seq{};  ///< bits 0..7, raw 0..255
    std::uint32_t payload{};  ///< bits 8..39, raw 0..4294967295
    std::uint16_t turnaround{};  ///< bits 40..55 [us], raw 0..65535

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(seq));
        codec::put(d, 8, 32, static_cast<std::uint32_t>(payload));
        codec::put(d, 40, 16, static_cast<std::uint32_t>(turnaround));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr SensorEchoResponse from_frame(const Frame& d)
    {
        SensorEchoResponse m{};
        m.seq = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.payload = static_cast<std::uint32_t>(codec::get_unsigned(d, 8, 32));
        m.turnaround = static_cast<std::uint16_t>(codec::get_unsigned(d, 40, 16));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, SensorEchoResponse& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 3> kFields = {{
        {"seq", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"payload", 8, 32, false, 1.0F, 0.0F, 0, 4294967295, ""},
        {"turnaround", 40, 16, false, 1.0F, 0.0F, 0, 65535, "us"},
    }};
};

constexpr bool operator==(const SensorEchoResponse& a, const SensorEchoResponse& b)
{
    return a.seq == b.seq
        && a.payload == b.payload
        && a.turnaround == b.turnaround;
}
constexpr bool operator!=(const SensorEchoResponse& a, const SensorEchoResponse& b)
{
    return !(a == b);
}

/// Controller ECU's answer to ECHO_REQUEST, same layout as SENSOR_ECHO_RESPONSE.
/// Senders: CONTROLLER_ECU
struct ControllerEchoResponse {
    static constexpr std::uint32_t kId = 0x302U;
    static constexpr std::size_t kDlc = 7;
    static constexpr std::uint16_t kCycleMs = 0;
    static constexpr bool kSafetyRelevant = false;
    static constexpr bool kTestOnly = false;
    static constexpr const char* kName = "CONTROLLER_ECHO_RESPONSE";
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint8_t seq{};  ///< bits 0..7, raw 0..255
    std::uint32_t payload{};  ///< bits 8..39, raw 0..4294967295
    std::uint16_t turnaround{};  ///< bits 40..55 [us], raw 0..65535

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 8, static_cast<std::uint32_t>(seq));
        codec::put(d, 8, 32, static_cast<std::uint32_t>(payload));
        codec::put(d, 40, 16, static_cast<std::uint32_t>(turnaround));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr ControllerEchoResponse from_frame(const Frame& d)
    {
        ControllerEchoResponse m{};
        m.seq = static_cast<std::uint8_t>(codec::get_unsigned(d, 0, 8));
        m.payload = static_cast<std::uint32_t>(codec::get_unsigned(d, 8, 32));
        m.turnaround = static_cast<std::uint16_t>(codec::get_unsigned(d, 40, 16));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, ControllerEchoResponse& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 3> kFields = {{
        {"seq", 0, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"payload", 8, 32, false, 1.0F, 0.0F, 0, 4294967295, ""},
        {"turnaround", 40, 16, false, 1.0F, 0.0F, 0, 65535, "us"},
    }};
};

constexpr bool operator==(const ControllerEchoResponse& a, const ControllerEchoResponse& b)
{
    return a.seq == b.seq
        && a.payload == b.payload
        && a.turnaround == b.turnaround;
}
constexpr bool operator!=(const ControllerEchoResponse& a, const ControllerEchoResponse& b)
{
    return !(a == b);
}

/// Test-only control message. Accepted only by firmware built with the test hooks enabled, and only when magic equals 0xC0DE. Drives the software fault-injection hooks (Phase 3).
/// Senders: TEST_HOST
struct TestControl {
    static constexpr std::uint32_t kId = 0x7E0U;
    static constexpr std::size_t kDlc = 8;
    static constexpr std::uint16_t kCycleMs = 0;
    static constexpr bool kSafetyRelevant = false;
    static constexpr bool kTestOnly = true;
    static constexpr const char* kName = "TEST_CONTROL";
    static constexpr std::uint16_t kMagic = 0xC0DEU;
    using Frame = std::array<std::uint8_t, kDlc>;

    std::uint16_t magic{};  ///< bits 0..15, raw 0..65535
    Node target_node{};  ///< bits 16..23, raw 0..255
    TestCommand command{};  ///< bits 24..31, raw 0..255. See value table. arg0/arg1 semantics depend on the command.
    std::uint16_t arg0{};  ///< bits 32..47, raw 0..65535
    std::uint16_t arg1{};  ///< bits 48..63, raw 0..65535

    /// Encode the raw field values into a frame.
    constexpr Frame to_frame() const
    {
        Frame d{};
        codec::put(d, 0, 16, static_cast<std::uint32_t>(magic));
        codec::put(d, 16, 8, static_cast<std::uint32_t>(target_node));
        codec::put(d, 24, 8, static_cast<std::uint32_t>(command));
        codec::put(d, 32, 16, static_cast<std::uint32_t>(arg0));
        codec::put(d, 48, 16, static_cast<std::uint32_t>(arg1));
        return d;
    }

    /// Decode a frame of exactly kDlc bytes.
    static constexpr TestControl from_frame(const Frame& d)
    {
        TestControl m{};
        m.magic = static_cast<std::uint16_t>(codec::get_unsigned(d, 0, 16));
        m.target_node = static_cast<Node>(codec::get_unsigned(d, 16, 8));
        m.command = static_cast<TestCommand>(codec::get_unsigned(d, 24, 8));
        m.arg0 = static_cast<std::uint16_t>(codec::get_unsigned(d, 32, 16));
        m.arg1 = static_cast<std::uint16_t>(codec::get_unsigned(d, 48, 16));
        return m;
    }

    /// Decode from a received buffer; false if it is shorter than kDlc.
    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, TestControl& out)
    {
        if (data == nullptr || len < kDlc) {
            return false;
        }
        Frame d{};
        for (std::size_t i = 0; i < kDlc; ++i) {
            d[i] = data[i];
        }
        out = from_frame(d);
        return true;
    }

    static constexpr std::array<FieldInfo, 5> kFields = {{
        {"magic", 0, 16, false, 1.0F, 0.0F, 0, 65535, ""},
        {"target_node", 16, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"command", 24, 8, false, 1.0F, 0.0F, 0, 255, ""},
        {"arg0", 32, 16, false, 1.0F, 0.0F, 0, 65535, ""},
        {"arg1", 48, 16, false, 1.0F, 0.0F, 0, 65535, ""},
    }};
};

constexpr bool operator==(const TestControl& a, const TestControl& b)
{
    return a.magic == b.magic
        && a.target_node == b.target_node
        && a.command == b.command
        && a.arg0 == b.arg0
        && a.arg1 == b.arg1;
}
constexpr bool operator!=(const TestControl& a, const TestControl& b)
{
    return !(a == b);
}

/// All messages of the network, ordered by identifier (bus priority).
inline constexpr std::array<MessageInfo, 9> kMessages = {{
    {ControllerCommand::kId, ControllerCommand::kDlc, ControllerCommand::kCycleMs, ControllerCommand::kSafetyRelevant, ControllerCommand::kTestOnly, ControllerCommand::kName},
    {SensorData::kId, SensorData::kDlc, SensorData::kCycleMs, SensorData::kSafetyRelevant, SensorData::kTestOnly, SensorData::kName},
    {BatteryStatus::kId, BatteryStatus::kDlc, BatteryStatus::kCycleMs, BatteryStatus::kSafetyRelevant, BatteryStatus::kTestOnly, BatteryStatus::kName},
    {SensorHeartbeat::kId, SensorHeartbeat::kDlc, SensorHeartbeat::kCycleMs, SensorHeartbeat::kSafetyRelevant, SensorHeartbeat::kTestOnly, SensorHeartbeat::kName},
    {ControllerHeartbeat::kId, ControllerHeartbeat::kDlc, ControllerHeartbeat::kCycleMs, ControllerHeartbeat::kSafetyRelevant, ControllerHeartbeat::kTestOnly, ControllerHeartbeat::kName},
    {EchoRequest::kId, EchoRequest::kDlc, EchoRequest::kCycleMs, EchoRequest::kSafetyRelevant, EchoRequest::kTestOnly, EchoRequest::kName},
    {SensorEchoResponse::kId, SensorEchoResponse::kDlc, SensorEchoResponse::kCycleMs, SensorEchoResponse::kSafetyRelevant, SensorEchoResponse::kTestOnly, SensorEchoResponse::kName},
    {ControllerEchoResponse::kId, ControllerEchoResponse::kDlc, ControllerEchoResponse::kCycleMs, ControllerEchoResponse::kSafetyRelevant, ControllerEchoResponse::kTestOnly, ControllerEchoResponse::kName},
    {TestControl::kId, TestControl::kDlc, TestControl::kCycleMs, TestControl::kSafetyRelevant, TestControl::kTestOnly, TestControl::kName},
}};

constexpr const MessageInfo* find_message(std::uint32_t id)
{
    for (const auto& m : kMessages) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

}  // namespace proto

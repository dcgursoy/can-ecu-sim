// GENERATED FILE, do not edit.
// Source: can/network.dbc  (sha256 c239c44a1c23952d)
// Generator: can/gen_test_vectors.py  (seed 20261025, cantools 40.7.1)
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol/messages.hpp"

namespace vectors {

/// Index of each signal of CONTROLLER_COMMAND in the phys array (DBC order).
struct ControllerCommandIdx {
    static constexpr std::size_t crc = 0;
    static constexpr std::size_t counter = 1;
    static constexpr std::size_t mode = 2;
    static constexpr std::size_t torque_cmd = 3;
    static constexpr std::size_t steering_cmd = 4;
    static constexpr std::size_t actuators_enabled = 5;
    static constexpr std::size_t brake_hold = 6;
    static constexpr std::size_t fault_latched = 7;
    static constexpr std::size_t speed_limit = 8;
};

struct ControllerCommandVector {
    proto::ControllerCommand::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::ControllerCommand expected;            ///< the raw values that were encoded
    std::array<double, 9> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<ControllerCommandVector, 32> kControllerCommand = {{
    {{0x00, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00}, [] { proto::ControllerCommand m{}; m.crc = 0; m.counter = 0; m.mode = static_cast<proto::ControllerMode>(0); m.torque_cmd = -32768; m.steering_cmd = -32768; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 0; return m; }(), {0.0, 0.0, 0.0, -327.68, -327.68, 0.0, 0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x7F, 0x07, 0xFF}, [] { proto::ControllerCommand m{}; m.crc = 255; m.counter = 15; m.mode = static_cast<proto::ControllerMode>(15); m.torque_cmd = 32767; m.steering_cmd = 32767; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 255; return m; }(), {255.0, 15.0, 15.0, 327.67, 327.67, 1.0, 1.0, 1.0, 25.5}},
    {{0xD9, 0x7C, 0x45, 0xE4, 0x2B, 0x63, 0x06, 0x49}, [] { proto::ControllerCommand m{}; m.crc = 217; m.counter = 12; m.mode = static_cast<proto::ControllerMode>(7); m.torque_cmd = -7099; m.steering_cmd = 25387; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 73; return m; }(), {217.0, 12.0, 7.0, -70.99, 253.87, 0.0, 1.0, 1.0, 7.300000000000001}},
    {{0x45, 0xA1, 0xEF, 0x61, 0x52, 0x47, 0x02, 0xF7}, [] { proto::ControllerCommand m{}; m.crc = 69; m.counter = 1; m.mode = static_cast<proto::ControllerMode>(10); m.torque_cmd = 25071; m.steering_cmd = 18258; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 247; return m; }(), {69.0, 1.0, 10.0, 250.71, 182.58, 0.0, 1.0, 0.0, 24.700000000000003}},
    {{0x16, 0x31, 0x27, 0x90, 0xFF, 0x03, 0x03, 0xFC}, [] { proto::ControllerCommand m{}; m.crc = 22; m.counter = 1; m.mode = static_cast<proto::ControllerMode>(3); m.torque_cmd = -28633; m.steering_cmd = 1023; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 252; return m; }(), {22.0, 1.0, 3.0, -286.33, 10.23, 1.0, 1.0, 0.0, 25.200000000000003}},
    {{0x14, 0xA0, 0xB6, 0xF4, 0xDD, 0x46, 0x07, 0x73}, [] { proto::ControllerCommand m{}; m.crc = 20; m.counter = 0; m.mode = static_cast<proto::ControllerMode>(10); m.torque_cmd = -2890; m.steering_cmd = 18141; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 115; return m; }(), {20.0, 0.0, 10.0, -28.900000000000002, 181.41, 1.0, 1.0, 1.0, 11.5}},
    {{0x44, 0xEE, 0x01, 0x9A, 0xA5, 0x39, 0x01, 0xA2}, [] { proto::ControllerCommand m{}; m.crc = 68; m.counter = 14; m.mode = static_cast<proto::ControllerMode>(14); m.torque_cmd = -26111; m.steering_cmd = 14757; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 162; return m; }(), {68.0, 14.0, 14.0, -261.11, 147.57, 1.0, 0.0, 0.0, 16.2}},
    {{0x54, 0x9A, 0x9A, 0x4C, 0x71, 0x3C, 0x03, 0x14}, [] { proto::ControllerCommand m{}; m.crc = 84; m.counter = 10; m.mode = static_cast<proto::ControllerMode>(9); m.torque_cmd = 19610; m.steering_cmd = 15473; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 20; return m; }(), {84.0, 10.0, 9.0, 196.1, 154.73, 1.0, 1.0, 0.0, 2.0}},
    {{0x3D, 0x42, 0x47, 0x42, 0x7B, 0x9F, 0x07, 0x72}, [] { proto::ControllerCommand m{}; m.crc = 61; m.counter = 2; m.mode = static_cast<proto::ControllerMode>(4); m.torque_cmd = 16967; m.steering_cmd = -24709; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 114; return m; }(), {61.0, 2.0, 4.0, 169.67000000000002, -247.09, 1.0, 1.0, 1.0, 11.4}},
    {{0x5D, 0xC8, 0xD2, 0xA8, 0x4A, 0xD9, 0x04, 0xD5}, [] { proto::ControllerCommand m{}; m.crc = 93; m.counter = 8; m.mode = static_cast<proto::ControllerMode>(12); m.torque_cmd = -22318; m.steering_cmd = -9910; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 213; return m; }(), {93.0, 8.0, 12.0, -223.18, -99.10000000000001, 0.0, 0.0, 1.0, 21.3}},
    {{0xBC, 0xF9, 0xDC, 0xF1, 0xC1, 0x80, 0x06, 0x73}, [] { proto::ControllerCommand m{}; m.crc = 188; m.counter = 9; m.mode = static_cast<proto::ControllerMode>(15); m.torque_cmd = -3620; m.steering_cmd = -32575; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 115; return m; }(), {188.0, 9.0, 15.0, -36.2, -325.75, 0.0, 1.0, 1.0, 11.5}},
    {{0xE9, 0x76, 0x86, 0xEA, 0x48, 0x2F, 0x00, 0x7D}, [] { proto::ControllerCommand m{}; m.crc = 233; m.counter = 6; m.mode = static_cast<proto::ControllerMode>(7); m.torque_cmd = -5498; m.steering_cmd = 12104; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 125; return m; }(), {233.0, 6.0, 7.0, -54.980000000000004, 121.04, 0.0, 0.0, 0.0, 12.5}},
    {{0x1C, 0x55, 0x08, 0x76, 0x53, 0x3F, 0x06, 0xE0}, [] { proto::ControllerCommand m{}; m.crc = 28; m.counter = 5; m.mode = static_cast<proto::ControllerMode>(5); m.torque_cmd = 30216; m.steering_cmd = 16211; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 224; return m; }(), {28.0, 5.0, 5.0, 302.16, 162.11, 0.0, 1.0, 1.0, 22.400000000000002}},
    {{0x1D, 0xE7, 0xF7, 0xFF, 0x86, 0xEC, 0x00, 0x4A}, [] { proto::ControllerCommand m{}; m.crc = 29; m.counter = 7; m.mode = static_cast<proto::ControllerMode>(14); m.torque_cmd = -9; m.steering_cmd = -4986; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 74; return m; }(), {29.0, 7.0, 14.0, -0.09, -49.86, 0.0, 0.0, 0.0, 7.4}},
    {{0xA9, 0x59, 0x7C, 0x21, 0x37, 0xE2, 0x06, 0xA2}, [] { proto::ControllerCommand m{}; m.crc = 169; m.counter = 9; m.mode = static_cast<proto::ControllerMode>(5); m.torque_cmd = 8572; m.steering_cmd = -7625; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 162; return m; }(), {169.0, 9.0, 5.0, 85.72, -76.25, 0.0, 1.0, 1.0, 16.2}},
    {{0xD8, 0xC8, 0x35, 0x51, 0x66, 0xE2, 0x05, 0xD5}, [] { proto::ControllerCommand m{}; m.crc = 216; m.counter = 8; m.mode = static_cast<proto::ControllerMode>(12); m.torque_cmd = 20789; m.steering_cmd = -7578; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 213; return m; }(), {216.0, 8.0, 12.0, 207.89000000000001, -75.78, 1.0, 0.0, 1.0, 21.3}},
    {{0xDC, 0x20, 0xA0, 0xE5, 0x1B, 0xE2, 0x02, 0x31}, [] { proto::ControllerCommand m{}; m.crc = 220; m.counter = 0; m.mode = static_cast<proto::ControllerMode>(2); m.torque_cmd = -6752; m.steering_cmd = -7653; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 49; return m; }(), {220.0, 0.0, 2.0, -67.52, -76.53, 0.0, 1.0, 0.0, 4.9}},
    {{0x3C, 0x03, 0xEB, 0x73, 0xD5, 0x42, 0x02, 0x20}, [] { proto::ControllerCommand m{}; m.crc = 60; m.counter = 3; m.mode = static_cast<proto::ControllerMode>(0); m.torque_cmd = 29675; m.steering_cmd = 17109; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 32; return m; }(), {60.0, 3.0, 0.0, 296.75, 171.09, 0.0, 1.0, 0.0, 3.2}},
    {{0x3B, 0x3E, 0xDD, 0xDC, 0xD6, 0x10, 0x05, 0x24}, [] { proto::ControllerCommand m{}; m.crc = 59; m.counter = 14; m.mode = static_cast<proto::ControllerMode>(3); m.torque_cmd = -8995; m.steering_cmd = 4310; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 36; return m; }(), {59.0, 14.0, 3.0, -89.95, 43.1, 1.0, 0.0, 1.0, 3.6}},
    {{0xEA, 0x83, 0x4B, 0xD1, 0xD4, 0x83, 0x00, 0x46}, [] { proto::ControllerCommand m{}; m.crc = 234; m.counter = 3; m.mode = static_cast<proto::ControllerMode>(8); m.torque_cmd = -11957; m.steering_cmd = -31788; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 70; return m; }(), {234.0, 3.0, 8.0, -119.57000000000001, -317.88, 0.0, 0.0, 0.0, 7.0}},
    {{0xC2, 0x5F, 0x89, 0xC6, 0x8D, 0x72, 0x05, 0x1A}, [] { proto::ControllerCommand m{}; m.crc = 194; m.counter = 15; m.mode = static_cast<proto::ControllerMode>(5); m.torque_cmd = -14711; m.steering_cmd = 29325; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 26; return m; }(), {194.0, 15.0, 5.0, -147.11, 293.25, 1.0, 0.0, 1.0, 2.6}},
    {{0x00, 0x1C, 0x16, 0xCF, 0xA6, 0xCE, 0x03, 0xB4}, [] { proto::ControllerCommand m{}; m.crc = 0; m.counter = 12; m.mode = static_cast<proto::ControllerMode>(1); m.torque_cmd = -12522; m.steering_cmd = -12634; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 180; return m; }(), {0.0, 12.0, 1.0, -125.22, -126.34, 1.0, 1.0, 0.0, 18.0}},
    {{0xAB, 0x88, 0x31, 0x6B, 0x12, 0x53, 0x07, 0x82}, [] { proto::ControllerCommand m{}; m.crc = 171; m.counter = 8; m.mode = static_cast<proto::ControllerMode>(8); m.torque_cmd = 27441; m.steering_cmd = 21266; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 130; return m; }(), {171.0, 8.0, 8.0, 274.41, 212.66, 1.0, 1.0, 1.0, 13.0}},
    {{0xF3, 0x4E, 0x10, 0xE1, 0x8A, 0x4E, 0x01, 0x21}, [] { proto::ControllerCommand m{}; m.crc = 243; m.counter = 14; m.mode = static_cast<proto::ControllerMode>(4); m.torque_cmd = -7920; m.steering_cmd = 20106; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 33; return m; }(), {243.0, 14.0, 4.0, -79.2, 201.06, 1.0, 0.0, 0.0, 3.3000000000000003}},
    {{0x42, 0x31, 0xA2, 0x5D, 0xB4, 0x57, 0x03, 0x89}, [] { proto::ControllerCommand m{}; m.crc = 66; m.counter = 1; m.mode = static_cast<proto::ControllerMode>(3); m.torque_cmd = 23970; m.steering_cmd = 22452; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 137; return m; }(), {66.0, 1.0, 3.0, 239.70000000000002, 224.52, 1.0, 1.0, 0.0, 13.700000000000001}},
    {{0x7B, 0x4F, 0xAA, 0x1A, 0x68, 0x21, 0x01, 0xDF}, [] { proto::ControllerCommand m{}; m.crc = 123; m.counter = 15; m.mode = static_cast<proto::ControllerMode>(4); m.torque_cmd = 6826; m.steering_cmd = 8552; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 0; m.speed_limit = 223; return m; }(), {123.0, 15.0, 4.0, 68.26, 85.52, 1.0, 0.0, 0.0, 22.3}},
    {{0x39, 0x15, 0x43, 0x8C, 0x73, 0x7E, 0x07, 0xCE}, [] { proto::ControllerCommand m{}; m.crc = 57; m.counter = 5; m.mode = static_cast<proto::ControllerMode>(1); m.torque_cmd = -29629; m.steering_cmd = 32371; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 206; return m; }(), {57.0, 5.0, 1.0, -296.29, 323.71, 1.0, 1.0, 1.0, 20.6}},
    {{0x13, 0xC4, 0xFB, 0xCB, 0x0C, 0x96, 0x04, 0x5F}, [] { proto::ControllerCommand m{}; m.crc = 19; m.counter = 4; m.mode = static_cast<proto::ControllerMode>(12); m.torque_cmd = -13317; m.steering_cmd = -27124; m.actuators_enabled = 0; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 95; return m; }(), {19.0, 4.0, 12.0, -133.17000000000002, -271.24, 0.0, 0.0, 1.0, 9.5}},
    {{0x8B, 0x5F, 0xFC, 0x2F, 0x6D, 0xB3, 0x02, 0x4C}, [] { proto::ControllerCommand m{}; m.crc = 139; m.counter = 15; m.mode = static_cast<proto::ControllerMode>(5); m.torque_cmd = 12284; m.steering_cmd = -19603; m.actuators_enabled = 0; m.brake_hold = 1; m.fault_latched = 0; m.speed_limit = 76; return m; }(), {139.0, 15.0, 5.0, 122.84, -196.03, 0.0, 1.0, 0.0, 7.6000000000000005}},
    {{0x7B, 0x9B, 0x19, 0xFC, 0xED, 0x22, 0x05, 0x0E}, [] { proto::ControllerCommand m{}; m.crc = 123; m.counter = 11; m.mode = static_cast<proto::ControllerMode>(9); m.torque_cmd = -999; m.steering_cmd = 8941; m.actuators_enabled = 1; m.brake_hold = 0; m.fault_latched = 1; m.speed_limit = 14; return m; }(), {123.0, 11.0, 9.0, -9.99, 89.41, 1.0, 0.0, 1.0, 1.4000000000000001}},
    {{0x46, 0x2C, 0xBE, 0x24, 0x6B, 0x66, 0x07, 0x7D}, [] { proto::ControllerCommand m{}; m.crc = 70; m.counter = 12; m.mode = static_cast<proto::ControllerMode>(2); m.torque_cmd = 9406; m.steering_cmd = 26219; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 125; return m; }(), {70.0, 12.0, 2.0, 94.06, 262.19, 1.0, 1.0, 1.0, 12.5}},
    {{0x35, 0xC0, 0xC4, 0xE8, 0x54, 0x93, 0x07, 0x8B}, [] { proto::ControllerCommand m{}; m.crc = 53; m.counter = 0; m.mode = static_cast<proto::ControllerMode>(12); m.torque_cmd = -5948; m.steering_cmd = -27820; m.actuators_enabled = 1; m.brake_hold = 1; m.fault_latched = 1; m.speed_limit = 139; return m; }(), {53.0, 0.0, 12.0, -59.480000000000004, -278.2, 1.0, 1.0, 1.0, 13.9}},
}};

/// Index of each signal of SENSOR_DATA in the phys array (DBC order).
struct SensorDataIdx {
    static constexpr std::size_t crc = 0;
    static constexpr std::size_t counter = 1;
    static constexpr std::size_t sensor_status = 2;
    static constexpr std::size_t speed = 3;
    static constexpr std::size_t yaw_rate = 4;
    static constexpr std::size_t accel_x = 5;
};

struct SensorDataVector {
    proto::SensorData::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::SensorData expected;            ///< the raw values that were encoded
    std::array<double, 6> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<SensorDataVector, 32> kSensorData = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80}, [] { proto::SensorData m{}; m.crc = 0; m.counter = 0; m.sensor_status = static_cast<proto::SensorStatus>(0); m.speed = 0; m.yaw_rate = -32768; m.accel_x = -32768; return m; }(), {0.0, 0.0, 0.0, 0.0, -3276.8, -327.68}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0x7F}, [] { proto::SensorData m{}; m.crc = 255; m.counter = 15; m.sensor_status = static_cast<proto::SensorStatus>(15); m.speed = 65535; m.yaw_rate = 32767; m.accel_x = 32767; return m; }(), {255.0, 15.0, 15.0, 655.35, 3276.7000000000003, 327.67}},
    {{0xB6, 0xB5, 0xDD, 0x31, 0xA9, 0xAA, 0x6F, 0x3E}, [] { proto::SensorData m{}; m.crc = 182; m.counter = 5; m.sensor_status = static_cast<proto::SensorStatus>(11); m.speed = 12765; m.yaw_rate = -21847; m.accel_x = 15983; return m; }(), {182.0, 5.0, 11.0, 127.65, -2184.7000000000003, 159.83}},
    {{0x11, 0x44, 0x49, 0xFE, 0x96, 0x70, 0x3A, 0x58}, [] { proto::SensorData m{}; m.crc = 17; m.counter = 4; m.sensor_status = static_cast<proto::SensorStatus>(4); m.speed = 65097; m.yaw_rate = 28822; m.accel_x = 22586; return m; }(), {17.0, 4.0, 4.0, 650.97, 2882.2000000000003, 225.86}},
    {{0xFB, 0xE2, 0x4C, 0x5A, 0x54, 0xDA, 0x85, 0x4D}, [] { proto::SensorData m{}; m.crc = 251; m.counter = 2; m.sensor_status = static_cast<proto::SensorStatus>(14); m.speed = 23116; m.yaw_rate = -9644; m.accel_x = 19845; return m; }(), {251.0, 2.0, 14.0, 231.16, -964.4000000000001, 198.45000000000002}},
    {{0xDC, 0x2B, 0x2A, 0x30, 0x77, 0x24, 0x5D, 0x46}, [] { proto::SensorData m{}; m.crc = 220; m.counter = 11; m.sensor_status = static_cast<proto::SensorStatus>(2); m.speed = 12330; m.yaw_rate = 9335; m.accel_x = 18013; return m; }(), {220.0, 11.0, 2.0, 123.3, 933.5, 180.13}},
    {{0xB2, 0xC1, 0x2F, 0xC4, 0x9A, 0xA0, 0x64, 0xDD}, [] { proto::SensorData m{}; m.crc = 178; m.counter = 1; m.sensor_status = static_cast<proto::SensorStatus>(12); m.speed = 50223; m.yaw_rate = -24422; m.accel_x = -8860; return m; }(), {178.0, 1.0, 12.0, 502.23, -2442.2000000000003, -88.60000000000001}},
    {{0x9D, 0x92, 0x36, 0x13, 0xDB, 0x9A, 0xC8, 0x92}, [] { proto::SensorData m{}; m.crc = 157; m.counter = 2; m.sensor_status = static_cast<proto::SensorStatus>(9); m.speed = 4918; m.yaw_rate = -25893; m.accel_x = -27960; return m; }(), {157.0, 2.0, 9.0, 49.18, -2589.3, -279.6}},
    {{0x8A, 0x1B, 0x00, 0x65, 0x6B, 0x17, 0x30, 0x4C}, [] { proto::SensorData m{}; m.crc = 138; m.counter = 11; m.sensor_status = static_cast<proto::SensorStatus>(1); m.speed = 25856; m.yaw_rate = 5995; m.accel_x = 19504; return m; }(), {138.0, 11.0, 1.0, 258.56, 599.5, 195.04}},
    {{0xA0, 0x1E, 0xAA, 0xA4, 0x45, 0xF3, 0x60, 0x47}, [] { proto::SensorData m{}; m.crc = 160; m.counter = 14; m.sensor_status = static_cast<proto::SensorStatus>(1); m.speed = 42154; m.yaw_rate = -3259; m.accel_x = 18272; return m; }(), {160.0, 14.0, 1.0, 421.54, -325.90000000000003, 182.72}},
    {{0x79, 0x07, 0x5F, 0xA4, 0x7C, 0xA0, 0xA7, 0x18}, [] { proto::SensorData m{}; m.crc = 121; m.counter = 7; m.sensor_status = static_cast<proto::SensorStatus>(0); m.speed = 42079; m.yaw_rate = -24452; m.accel_x = 6311; return m; }(), {121.0, 7.0, 0.0, 420.79, -2445.2000000000003, 63.11}},
    {{0xF5, 0x55, 0x7F, 0x42, 0x80, 0xAB, 0x7A, 0x40}, [] { proto::SensorData m{}; m.crc = 245; m.counter = 5; m.sensor_status = static_cast<proto::SensorStatus>(5); m.speed = 17023; m.yaw_rate = -21632; m.accel_x = 16506; return m; }(), {245.0, 5.0, 5.0, 170.23, -2163.2000000000003, 165.06}},
    {{0x37, 0x83, 0x1B, 0x5B, 0x91, 0xE0, 0xAC, 0x0C}, [] { proto::SensorData m{}; m.crc = 55; m.counter = 3; m.sensor_status = static_cast<proto::SensorStatus>(8); m.speed = 23323; m.yaw_rate = -8047; m.accel_x = 3244; return m; }(), {55.0, 3.0, 8.0, 233.23000000000002, -804.7, 32.44}},
    {{0xD4, 0xEE, 0xA7, 0xF4, 0xC0, 0x79, 0xF5, 0x7E}, [] { proto::SensorData m{}; m.crc = 212; m.counter = 14; m.sensor_status = static_cast<proto::SensorStatus>(14); m.speed = 62631; m.yaw_rate = 31168; m.accel_x = 32501; return m; }(), {212.0, 14.0, 14.0, 626.3100000000001, 3116.8, 325.01}},
    {{0xC6, 0xBD, 0xF5, 0xF0, 0x1B, 0xBE, 0x7A, 0x94}, [] { proto::SensorData m{}; m.crc = 198; m.counter = 13; m.sensor_status = static_cast<proto::SensorStatus>(11); m.speed = 61685; m.yaw_rate = -16869; m.accel_x = -27526; return m; }(), {198.0, 13.0, 11.0, 616.85, -1686.9, -275.26}},
    {{0x21, 0x5A, 0xE0, 0x46, 0x54, 0x71, 0x21, 0xD2}, [] { proto::SensorData m{}; m.crc = 33; m.counter = 10; m.sensor_status = static_cast<proto::SensorStatus>(5); m.speed = 18144; m.yaw_rate = 29012; m.accel_x = -11743; return m; }(), {33.0, 10.0, 5.0, 181.44, 2901.2000000000003, -117.43}},
    {{0x8B, 0x18, 0xBA, 0xBF, 0xDA, 0xF6, 0x22, 0xFD}, [] { proto::SensorData m{}; m.crc = 139; m.counter = 8; m.sensor_status = static_cast<proto::SensorStatus>(1); m.speed = 49082; m.yaw_rate = -2342; m.accel_x = -734; return m; }(), {139.0, 8.0, 1.0, 490.82, -234.20000000000002, -7.34}},
    {{0xDF, 0x57, 0x14, 0x5B, 0xC6, 0x9E, 0xD8, 0xF4}, [] { proto::SensorData m{}; m.crc = 223; m.counter = 7; m.sensor_status = static_cast<proto::SensorStatus>(5); m.speed = 23316; m.yaw_rate = -24890; m.accel_x = -2856; return m; }(), {223.0, 7.0, 5.0, 233.16, -2489.0, -28.560000000000002}},
    {{0x63, 0x68, 0x27, 0x53, 0x54, 0x84, 0x86, 0x7F}, [] { proto::SensorData m{}; m.crc = 99; m.counter = 8; m.sensor_status = static_cast<proto::SensorStatus>(6); m.speed = 21287; m.yaw_rate = -31660; m.accel_x = 32646; return m; }(), {99.0, 8.0, 6.0, 212.87, -3166.0, 326.46}},
    {{0x61, 0xBA, 0x42, 0xF9, 0xC8, 0x66, 0xAA, 0x66}, [] { proto::SensorData m{}; m.crc = 97; m.counter = 10; m.sensor_status = static_cast<proto::SensorStatus>(11); m.speed = 63810; m.yaw_rate = 26312; m.accel_x = 26282; return m; }(), {97.0, 10.0, 11.0, 638.1, 2631.2000000000003, 262.82}},
    {{0x7D, 0x55, 0x5A, 0x4A, 0xD4, 0x74, 0x65, 0x61}, [] { proto::SensorData m{}; m.crc = 125; m.counter = 5; m.sensor_status = static_cast<proto::SensorStatus>(5); m.speed = 19034; m.yaw_rate = 29908; m.accel_x = 24933; return m; }(), {125.0, 5.0, 5.0, 190.34, 2990.8, 249.33}},
    {{0xAB, 0xCB, 0xD0, 0x74, 0xA3, 0x10, 0x69, 0x9F}, [] { proto::SensorData m{}; m.crc = 171; m.counter = 11; m.sensor_status = static_cast<proto::SensorStatus>(12); m.speed = 29904; m.yaw_rate = 4259; m.accel_x = -24727; return m; }(), {171.0, 11.0, 12.0, 299.04, 425.90000000000003, -247.27}},
    {{0x4B, 0xBC, 0x67, 0x9A, 0x34, 0x90, 0xEF, 0xA9}, [] { proto::SensorData m{}; m.crc = 75; m.counter = 12; m.sensor_status = static_cast<proto::SensorStatus>(11); m.speed = 39527; m.yaw_rate = -28620; m.accel_x = -22033; return m; }(), {75.0, 12.0, 11.0, 395.27, -2862.0, -220.33}},
    {{0x70, 0x3C, 0x05, 0xED, 0xDB, 0xFC, 0x81, 0xCD}, [] { proto::SensorData m{}; m.crc = 112; m.counter = 12; m.sensor_status = static_cast<proto::SensorStatus>(3); m.speed = 60677; m.yaw_rate = -805; m.accel_x = -12927; return m; }(), {112.0, 12.0, 3.0, 606.77, -80.5, -129.27}},
    {{0x3A, 0x18, 0xAC, 0x32, 0xBE, 0x20, 0xBE, 0xCE}, [] { proto::SensorData m{}; m.crc = 58; m.counter = 8; m.sensor_status = static_cast<proto::SensorStatus>(1); m.speed = 12972; m.yaw_rate = 8382; m.accel_x = -12610; return m; }(), {58.0, 8.0, 1.0, 129.72, 838.2, -126.10000000000001}},
    {{0x5F, 0x4C, 0x87, 0x5D, 0x24, 0x2F, 0xBF, 0x58}, [] { proto::SensorData m{}; m.crc = 95; m.counter = 12; m.sensor_status = static_cast<proto::SensorStatus>(4); m.speed = 23943; m.yaw_rate = 12068; m.accel_x = 22719; return m; }(), {95.0, 12.0, 4.0, 239.43, 1206.8, 227.19}},
    {{0xBA, 0x8C, 0x20, 0xA9, 0xD4, 0x15, 0x7B, 0x09}, [] { proto::SensorData m{}; m.crc = 186; m.counter = 12; m.sensor_status = static_cast<proto::SensorStatus>(8); m.speed = 43296; m.yaw_rate = 5588; m.accel_x = 2427; return m; }(), {186.0, 12.0, 8.0, 432.96000000000004, 558.8000000000001, 24.27}},
    {{0xB7, 0x47, 0xAF, 0x6C, 0x7A, 0x6E, 0x42, 0xD6}, [] { proto::SensorData m{}; m.crc = 183; m.counter = 7; m.sensor_status = static_cast<proto::SensorStatus>(4); m.speed = 27823; m.yaw_rate = 28282; m.accel_x = -10686; return m; }(), {183.0, 7.0, 4.0, 278.23, 2828.2000000000003, -106.86}},
    {{0xF4, 0xD0, 0x3C, 0xD4, 0x58, 0xD8, 0x46, 0x24}, [] { proto::SensorData m{}; m.crc = 244; m.counter = 0; m.sensor_status = static_cast<proto::SensorStatus>(13); m.speed = 54332; m.yaw_rate = -10152; m.accel_x = 9286; return m; }(), {244.0, 0.0, 13.0, 543.32, -1015.2, 92.86}},
    {{0xA2, 0x0B, 0x6B, 0x86, 0xA5, 0x4B, 0x6B, 0xD1}, [] { proto::SensorData m{}; m.crc = 162; m.counter = 11; m.sensor_status = static_cast<proto::SensorStatus>(0); m.speed = 34411; m.yaw_rate = 19365; m.accel_x = -11925; return m; }(), {162.0, 11.0, 0.0, 344.11, 1936.5, -119.25}},
    {{0xCC, 0x1D, 0xE6, 0x35, 0x93, 0x92, 0x2E, 0x1A}, [] { proto::SensorData m{}; m.crc = 204; m.counter = 13; m.sensor_status = static_cast<proto::SensorStatus>(1); m.speed = 13798; m.yaw_rate = -28013; m.accel_x = 6702; return m; }(), {204.0, 13.0, 1.0, 137.98, -2801.3, 67.02}},
    {{0x94, 0xE4, 0x17, 0xC6, 0x10, 0x0A, 0x9D, 0x84}, [] { proto::SensorData m{}; m.crc = 148; m.counter = 4; m.sensor_status = static_cast<proto::SensorStatus>(14); m.speed = 50711; m.yaw_rate = 2576; m.accel_x = -31587; return m; }(), {148.0, 4.0, 14.0, 507.11, 257.6, -315.87}},
}};

/// Index of each signal of BATTERY_STATUS in the phys array (DBC order).
struct BatteryStatusIdx {
    static constexpr std::size_t crc = 0;
    static constexpr std::size_t counter = 1;
    static constexpr std::size_t charging = 2;
    static constexpr std::size_t low_voltage = 3;
    static constexpr std::size_t over_temperature = 4;
    static constexpr std::size_t pack_voltage = 5;
    static constexpr std::size_t pack_current = 6;
    static constexpr std::size_t state_of_charge = 7;
    static constexpr std::size_t temperature = 8;
};

struct BatteryStatusVector {
    proto::BatteryStatus::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::BatteryStatus expected;            ///< the raw values that were encoded
    std::array<double, 9> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<BatteryStatusVector, 32> kBatteryStatus = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00}, [] { proto::BatteryStatus m{}; m.crc = 0; m.counter = 0; m.charging = 0; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 0; m.pack_current = -32768; m.state_of_charge = 0; m.temperature = 0; return m; }(), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -3276.8, 0.0, -40.0}},
    {{0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xC8, 0xFF}, [] { proto::BatteryStatus m{}; m.crc = 255; m.counter = 15; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 65535; m.pack_current = 32767; m.state_of_charge = 200; m.temperature = 255; return m; }(), {255.0, 15.0, 1.0, 1.0, 1.0, 655.35, 3276.7000000000003, 100.0, 215.0}},
    {{0x7F, 0x75, 0xD7, 0x7C, 0xCA, 0x87, 0x52, 0x9A}, [] { proto::BatteryStatus m{}; m.crc = 127; m.counter = 5; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 31959; m.pack_current = -30774; m.state_of_charge = 82; m.temperature = 154; return m; }(), {127.0, 5.0, 1.0, 1.0, 1.0, 319.59000000000003, -3077.4, 41.0, 114.0}},
    {{0x70, 0x71, 0x45, 0x75, 0xC2, 0x19, 0xB4, 0x15}, [] { proto::BatteryStatus m{}; m.crc = 112; m.counter = 1; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 30021; m.pack_current = 6594; m.state_of_charge = 180; m.temperature = 21; return m; }(), {112.0, 1.0, 1.0, 1.0, 1.0, 300.21, 659.4000000000001, 90.0, -19.0}},
    {{0xD1, 0x2B, 0xCB, 0x2D, 0x70, 0x7B, 0x65, 0xCE}, [] { proto::BatteryStatus m{}; m.crc = 209; m.counter = 11; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 11723; m.pack_current = 31600; m.state_of_charge = 101; m.temperature = 206; return m; }(), {209.0, 11.0, 0.0, 1.0, 0.0, 117.23, 3160.0, 50.5, 166.0}},
    {{0xE5, 0x29, 0xA4, 0xCD, 0x49, 0x2A, 0x82, 0x24}, [] { proto::BatteryStatus m{}; m.crc = 229; m.counter = 9; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 52644; m.pack_current = 10825; m.state_of_charge = 130; m.temperature = 36; return m; }(), {229.0, 9.0, 0.0, 1.0, 0.0, 526.44, 1082.5, 65.0, -4.0}},
    {{0x71, 0x55, 0x5D, 0x51, 0x4A, 0xC9, 0x94, 0x0B}, [] { proto::BatteryStatus m{}; m.crc = 113; m.counter = 5; m.charging = 1; m.low_voltage = 0; m.over_temperature = 1; m.pack_voltage = 20829; m.pack_current = -14006; m.state_of_charge = 148; m.temperature = 11; return m; }(), {113.0, 5.0, 1.0, 0.0, 1.0, 208.29, -1400.6000000000001, 74.0, -29.0}},
    {{0x59, 0x61, 0x3C, 0x0A, 0x6E, 0xEE, 0x43, 0x0F}, [] { proto::BatteryStatus m{}; m.crc = 89; m.counter = 1; m.charging = 0; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 2620; m.pack_current = -4498; m.state_of_charge = 67; m.temperature = 15; return m; }(), {89.0, 1.0, 0.0, 1.0, 1.0, 26.2, -449.8, 33.5, -25.0}},
    {{0x58, 0x20, 0x32, 0x08, 0x4F, 0x8C, 0x12, 0x4C}, [] { proto::BatteryStatus m{}; m.crc = 88; m.counter = 0; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 2098; m.pack_current = -29617; m.state_of_charge = 18; m.temperature = 76; return m; }(), {88.0, 0.0, 0.0, 1.0, 0.0, 20.98, -2961.7000000000003, 9.0, 36.0}},
    {{0x21, 0x54, 0x46, 0xD4, 0xBC, 0x40, 0xAA, 0x4D}, [] { proto::BatteryStatus m{}; m.crc = 33; m.counter = 4; m.charging = 1; m.low_voltage = 0; m.over_temperature = 1; m.pack_voltage = 54342; m.pack_current = 16572; m.state_of_charge = 170; m.temperature = 77; return m; }(), {33.0, 4.0, 1.0, 0.0, 1.0, 543.42, 1657.2, 85.0, 37.0}},
    {{0x26, 0x38, 0x0E, 0xBD, 0x41, 0x7D, 0x5A, 0x4E}, [] { proto::BatteryStatus m{}; m.crc = 38; m.counter = 8; m.charging = 1; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 48398; m.pack_current = 32065; m.state_of_charge = 90; m.temperature = 78; return m; }(), {38.0, 8.0, 1.0, 1.0, 0.0, 483.98, 3206.5, 45.0, 38.0}},
    {{0xBB, 0x12, 0x76, 0xD8, 0x5D, 0x2A, 0xB4, 0xE0}, [] { proto::BatteryStatus m{}; m.crc = 187; m.counter = 2; m.charging = 1; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 55414; m.pack_current = 10845; m.state_of_charge = 180; m.temperature = 224; return m; }(), {187.0, 2.0, 1.0, 0.0, 0.0, 554.14, 1084.5, 90.0, 184.0}},
    {{0x68, 0x23, 0x49, 0xD3, 0xFA, 0x69, 0x10, 0xAC}, [] { proto::BatteryStatus m{}; m.crc = 104; m.counter = 3; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 54089; m.pack_current = 27130; m.state_of_charge = 16; m.temperature = 172; return m; }(), {104.0, 3.0, 0.0, 1.0, 0.0, 540.89, 2713.0, 8.0, 132.0}},
    {{0xE4, 0x35, 0xFF, 0xE0, 0x89, 0x66, 0xA6, 0x0E}, [] { proto::BatteryStatus m{}; m.crc = 228; m.counter = 5; m.charging = 1; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 57599; m.pack_current = 26249; m.state_of_charge = 166; m.temperature = 14; return m; }(), {228.0, 5.0, 1.0, 1.0, 0.0, 575.99, 2624.9, 83.0, -26.0}},
    {{0x86, 0x7E, 0x51, 0x31, 0x93, 0x06, 0xB0, 0xF6}, [] { proto::BatteryStatus m{}; m.crc = 134; m.counter = 14; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 12625; m.pack_current = 1683; m.state_of_charge = 176; m.temperature = 246; return m; }(), {134.0, 14.0, 1.0, 1.0, 1.0, 126.25, 168.3, 88.0, 206.0}},
    {{0xD2, 0x47, 0xD0, 0xF8, 0xAF, 0x60, 0x98, 0x99}, [] { proto::BatteryStatus m{}; m.crc = 210; m.counter = 7; m.charging = 0; m.low_voltage = 0; m.over_temperature = 1; m.pack_voltage = 63696; m.pack_current = 24751; m.state_of_charge = 152; m.temperature = 153; return m; }(), {210.0, 7.0, 0.0, 0.0, 1.0, 636.96, 2475.1000000000004, 76.0, 113.0}},
    {{0xC3, 0x39, 0x5C, 0xBC, 0xE7, 0x30, 0x1D, 0x13}, [] { proto::BatteryStatus m{}; m.crc = 195; m.counter = 9; m.charging = 1; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 48220; m.pack_current = 12519; m.state_of_charge = 29; m.temperature = 19; return m; }(), {195.0, 9.0, 1.0, 1.0, 0.0, 482.2, 1251.9, 14.5, -21.0}},
    {{0x0F, 0x19, 0xCE, 0x73, 0x0C, 0xF1, 0xC5, 0x14}, [] { proto::BatteryStatus m{}; m.crc = 15; m.counter = 9; m.charging = 1; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 29646; m.pack_current = -3828; m.state_of_charge = 197; m.temperature = 20; return m; }(), {15.0, 9.0, 1.0, 0.0, 0.0, 296.46, -382.8, 98.5, -20.0}},
    {{0xEC, 0x22, 0xF7, 0x27, 0xC6, 0x3C, 0x69, 0xDD}, [] { proto::BatteryStatus m{}; m.crc = 236; m.counter = 2; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 10231; m.pack_current = 15558; m.state_of_charge = 105; m.temperature = 221; return m; }(), {236.0, 2.0, 0.0, 1.0, 0.0, 102.31, 1555.8000000000002, 52.5, 181.0}},
    {{0x5E, 0x7E, 0x29, 0x10, 0xF4, 0x7D, 0xBC, 0xE5}, [] { proto::BatteryStatus m{}; m.crc = 94; m.counter = 14; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 4137; m.pack_current = 32244; m.state_of_charge = 188; m.temperature = 229; return m; }(), {94.0, 14.0, 1.0, 1.0, 1.0, 41.37, 3224.4, 94.0, 189.0}},
    {{0xED, 0x2D, 0xFA, 0x4F, 0xA2, 0x28, 0x31, 0xD4}, [] { proto::BatteryStatus m{}; m.crc = 237; m.counter = 13; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 20474; m.pack_current = 10402; m.state_of_charge = 49; m.temperature = 212; return m; }(), {237.0, 13.0, 0.0, 1.0, 0.0, 204.74, 1040.2, 24.5, 172.0}},
    {{0xB0, 0x0C, 0x78, 0x3B, 0x42, 0xE6, 0x39, 0x8D}, [] { proto::BatteryStatus m{}; m.crc = 176; m.counter = 12; m.charging = 0; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 15224; m.pack_current = -6590; m.state_of_charge = 57; m.temperature = 141; return m; }(), {176.0, 12.0, 0.0, 0.0, 0.0, 152.24, -659.0, 28.5, 101.0}},
    {{0xB6, 0x0F, 0xBD, 0x80, 0xB0, 0xA0, 0x8F, 0x00}, [] { proto::BatteryStatus m{}; m.crc = 182; m.counter = 15; m.charging = 0; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 32957; m.pack_current = -24400; m.state_of_charge = 143; m.temperature = 0; return m; }(), {182.0, 15.0, 0.0, 0.0, 0.0, 329.57, -2440.0, 71.5, -40.0}},
    {{0xD0, 0x6A, 0x85, 0xC8, 0x56, 0x31, 0x62, 0x60}, [] { proto::BatteryStatus m{}; m.crc = 208; m.counter = 10; m.charging = 0; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 51333; m.pack_current = 12630; m.state_of_charge = 98; m.temperature = 96; return m; }(), {208.0, 10.0, 0.0, 1.0, 1.0, 513.33, 1263.0, 49.0, 56.0}},
    {{0xD9, 0x6A, 0x58, 0x1A, 0x8D, 0x73, 0x61, 0x4D}, [] { proto::BatteryStatus m{}; m.crc = 217; m.counter = 10; m.charging = 0; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 6744; m.pack_current = 29581; m.state_of_charge = 97; m.temperature = 77; return m; }(), {217.0, 10.0, 0.0, 1.0, 1.0, 67.44, 2958.1000000000004, 48.5, 37.0}},
    {{0x7A, 0x08, 0x42, 0xD4, 0x29, 0x3F, 0x92, 0x9F}, [] { proto::BatteryStatus m{}; m.crc = 122; m.counter = 8; m.charging = 0; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 54338; m.pack_current = 16169; m.state_of_charge = 146; m.temperature = 159; return m; }(), {122.0, 8.0, 0.0, 0.0, 0.0, 543.38, 1616.9, 73.0, 119.0}},
    {{0xB0, 0x5F, 0xF5, 0xEE, 0x3F, 0x04, 0x5A, 0xDF}, [] { proto::BatteryStatus m{}; m.crc = 176; m.counter = 15; m.charging = 1; m.low_voltage = 0; m.over_temperature = 1; m.pack_voltage = 61173; m.pack_current = 1087; m.state_of_charge = 90; m.temperature = 223; return m; }(), {176.0, 15.0, 1.0, 0.0, 1.0, 611.73, 108.7, 45.0, 183.0}},
    {{0xE3, 0x7F, 0xC2, 0xA9, 0xC2, 0x89, 0xA8, 0x3E}, [] { proto::BatteryStatus m{}; m.crc = 227; m.counter = 15; m.charging = 1; m.low_voltage = 1; m.over_temperature = 1; m.pack_voltage = 43458; m.pack_current = -30270; m.state_of_charge = 168; m.temperature = 62; return m; }(), {227.0, 15.0, 1.0, 1.0, 1.0, 434.58, -3027.0, 84.0, 22.0}},
    {{0x83, 0x30, 0x38, 0x98, 0xAA, 0xB2, 0xBE, 0xF4}, [] { proto::BatteryStatus m{}; m.crc = 131; m.counter = 0; m.charging = 1; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 38968; m.pack_current = -19798; m.state_of_charge = 190; m.temperature = 244; return m; }(), {131.0, 0.0, 1.0, 1.0, 0.0, 389.68, -1979.8000000000002, 95.0, 204.0}},
    {{0x3A, 0x06, 0x66, 0xF5, 0x1D, 0x2A, 0x6D, 0x9E}, [] { proto::BatteryStatus m{}; m.crc = 58; m.counter = 6; m.charging = 0; m.low_voltage = 0; m.over_temperature = 0; m.pack_voltage = 62822; m.pack_current = 10781; m.state_of_charge = 109; m.temperature = 158; return m; }(), {58.0, 6.0, 0.0, 0.0, 0.0, 628.22, 1078.1000000000001, 54.5, 118.0}},
    {{0xD0, 0x33, 0x7F, 0x22, 0xDC, 0x46, 0x21, 0x1C}, [] { proto::BatteryStatus m{}; m.crc = 208; m.counter = 3; m.charging = 1; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 8831; m.pack_current = 18140; m.state_of_charge = 33; m.temperature = 28; return m; }(), {208.0, 3.0, 1.0, 1.0, 0.0, 88.31, 1814.0, 16.5, -12.0}},
    {{0x5E, 0x2D, 0x06, 0xBD, 0x72, 0x0F, 0x7C, 0x91}, [] { proto::BatteryStatus m{}; m.crc = 94; m.counter = 13; m.charging = 0; m.low_voltage = 1; m.over_temperature = 0; m.pack_voltage = 48390; m.pack_current = 3954; m.state_of_charge = 124; m.temperature = 145; return m; }(), {94.0, 13.0, 0.0, 1.0, 0.0, 483.90000000000003, 395.40000000000003, 62.0, 105.0}},
}};

/// Index of each signal of SENSOR_HEARTBEAT in the phys array (DBC order).
struct SensorHeartbeatIdx {
    static constexpr std::size_t crc = 0;
    static constexpr std::size_t counter = 1;
    static constexpr std::size_t node_state = 2;
    static constexpr std::size_t uptime = 3;
    static constexpr std::size_t can_state = 4;
    static constexpr std::size_t bus_off_count = 5;
    static constexpr std::size_t error_flags = 6;
};

struct SensorHeartbeatVector {
    proto::SensorHeartbeat::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::SensorHeartbeat expected;            ///< the raw values that were encoded
    std::array<double, 7> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<SensorHeartbeatVector, 32> kSensorHeartbeat = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::SensorHeartbeat m{}; m.crc = 0; m.counter = 0; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 0; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 0; m.error_flags = 0; return m; }(), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::SensorHeartbeat m{}; m.crc = 255; m.counter = 15; m.node_state = static_cast<proto::NodeState>(15); m.uptime = 4294967295; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 63; m.error_flags = 255; return m; }(), {255.0, 15.0, 15.0, 4294967295.0, 3.0, 63.0, 255.0}},
    {{0x80, 0x47, 0x90, 0xC3, 0x45, 0x43, 0x0C, 0xBE}, [] { proto::SensorHeartbeat m{}; m.crc = 128; m.counter = 7; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 1128645520; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 3; m.error_flags = 190; return m; }(), {128.0, 7.0, 4.0, 1128645520.0, 0.0, 3.0, 190.0}},
    {{0xEB, 0x5A, 0x39, 0x9C, 0xE6, 0x88, 0xD4, 0xD5}, [] { proto::SensorHeartbeat m{}; m.crc = 235; m.counter = 10; m.node_state = static_cast<proto::NodeState>(5); m.uptime = 2296814649; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 53; m.error_flags = 213; return m; }(), {235.0, 10.0, 5.0, 2296814649.0, 0.0, 53.0, 213.0}},
    {{0x49, 0x4D, 0xF5, 0x6D, 0x20, 0x9E, 0xD5, 0xDE}, [] { proto::SensorHeartbeat m{}; m.crc = 73; m.counter = 13; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 2652925429; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 53; m.error_flags = 222; return m; }(), {73.0, 13.0, 4.0, 2652925429.0, 1.0, 53.0, 222.0}},
    {{0x38, 0x07, 0x94, 0xA5, 0xCD, 0x07, 0xB2, 0x6B}, [] { proto::SensorHeartbeat m{}; m.crc = 56; m.counter = 7; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 130917780; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 44; m.error_flags = 107; return m; }(), {56.0, 7.0, 0.0, 130917780.0, 2.0, 44.0, 107.0}},
    {{0x62, 0x29, 0x5A, 0x7E, 0x0A, 0xEA, 0xFF, 0x16}, [] { proto::SensorHeartbeat m{}; m.crc = 98; m.counter = 9; m.node_state = static_cast<proto::NodeState>(2); m.uptime = 3926556250; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 63; m.error_flags = 22; return m; }(), {98.0, 9.0, 2.0, 3926556250.0, 3.0, 63.0, 22.0}},
    {{0x89, 0xBD, 0xFE, 0x85, 0x6A, 0x69, 0x7F, 0x90}, [] { proto::SensorHeartbeat m{}; m.crc = 137; m.counter = 13; m.node_state = static_cast<proto::NodeState>(11); m.uptime = 1768588798; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 31; m.error_flags = 144; return m; }(), {137.0, 13.0, 11.0, 1768588798.0, 3.0, 31.0, 144.0}},
    {{0xAE, 0x69, 0x93, 0x75, 0xEA, 0x34, 0x80, 0x66}, [] { proto::SensorHeartbeat m{}; m.crc = 174; m.counter = 9; m.node_state = static_cast<proto::NodeState>(6); m.uptime = 887780755; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 32; m.error_flags = 102; return m; }(), {174.0, 9.0, 6.0, 887780755.0, 0.0, 32.0, 102.0}},
    {{0x61, 0x82, 0xD3, 0x77, 0xDC, 0x72, 0xB8, 0xC2}, [] { proto::SensorHeartbeat m{}; m.crc = 97; m.counter = 2; m.node_state = static_cast<proto::NodeState>(8); m.uptime = 1927051219; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 46; m.error_flags = 194; return m; }(), {97.0, 2.0, 8.0, 1927051219.0, 0.0, 46.0, 194.0}},
    {{0x3F, 0xC3, 0x70, 0xEC, 0x7E, 0xCB, 0xE3, 0x2A}, [] { proto::SensorHeartbeat m{}; m.crc = 63; m.counter = 3; m.node_state = static_cast<proto::NodeState>(12); m.uptime = 3414092912; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 56; m.error_flags = 42; return m; }(), {63.0, 3.0, 12.0, 3414092912.0, 3.0, 56.0, 42.0}},
    {{0x4B, 0xE5, 0x5D, 0xDC, 0xA1, 0xEE, 0x66, 0x17}, [] { proto::SensorHeartbeat m{}; m.crc = 75; m.counter = 5; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 4003585117; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 25; m.error_flags = 23; return m; }(), {75.0, 5.0, 14.0, 4003585117.0, 2.0, 25.0, 23.0}},
    {{0xCC, 0xB7, 0x3D, 0x63, 0x8A, 0x72, 0x34, 0x47}, [] { proto::SensorHeartbeat m{}; m.crc = 204; m.counter = 7; m.node_state = static_cast<proto::NodeState>(11); m.uptime = 1921671997; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 13; m.error_flags = 71; return m; }(), {204.0, 7.0, 11.0, 1921671997.0, 0.0, 13.0, 71.0}},
    {{0x65, 0x84, 0xA4, 0xD2, 0x31, 0xFE, 0x83, 0x4E}, [] { proto::SensorHeartbeat m{}; m.crc = 101; m.counter = 4; m.node_state = static_cast<proto::NodeState>(8); m.uptime = 4264678052; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 32; m.error_flags = 78; return m; }(), {101.0, 4.0, 8.0, 4264678052.0, 3.0, 32.0, 78.0}},
    {{0x19, 0x0F, 0x81, 0xEC, 0xCB, 0xE5, 0xBE, 0xC5}, [] { proto::SensorHeartbeat m{}; m.crc = 25; m.counter = 15; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 3855346817; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 47; m.error_flags = 197; return m; }(), {25.0, 15.0, 0.0, 3855346817.0, 2.0, 47.0, 197.0}},
    {{0x6E, 0xE6, 0xD3, 0x93, 0xC4, 0x1D, 0x1E, 0x4E}, [] { proto::SensorHeartbeat m{}; m.crc = 110; m.counter = 6; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 499422163; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 7; m.error_flags = 78; return m; }(), {110.0, 6.0, 14.0, 499422163.0, 2.0, 7.0, 78.0}},
    {{0x43, 0xE1, 0x05, 0xA8, 0x8F, 0x90, 0xA8, 0xC2}, [] { proto::SensorHeartbeat m{}; m.crc = 67; m.counter = 1; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 2425333765; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 42; m.error_flags = 194; return m; }(), {67.0, 1.0, 14.0, 2425333765.0, 0.0, 42.0, 194.0}},
    {{0x57, 0x91, 0x88, 0xA9, 0x72, 0xED, 0x03, 0xD8}, [] { proto::SensorHeartbeat m{}; m.crc = 87; m.counter = 1; m.node_state = static_cast<proto::NodeState>(9); m.uptime = 3983714696; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 0; m.error_flags = 216; return m; }(), {87.0, 1.0, 9.0, 3983714696.0, 3.0, 0.0, 216.0}},
    {{0xEA, 0xC4, 0xCD, 0x6B, 0x39, 0xD4, 0xD3, 0x0A}, [] { proto::SensorHeartbeat m{}; m.crc = 234; m.counter = 4; m.node_state = static_cast<proto::NodeState>(12); m.uptime = 3560532941; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 52; m.error_flags = 10; return m; }(), {234.0, 4.0, 12.0, 3560532941.0, 3.0, 52.0, 10.0}},
    {{0xE1, 0xEF, 0x26, 0x19, 0x41, 0x24, 0x8C, 0xA7}, [] { proto::SensorHeartbeat m{}; m.crc = 225; m.counter = 15; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 608246054; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 35; m.error_flags = 167; return m; }(), {225.0, 15.0, 14.0, 608246054.0, 0.0, 35.0, 167.0}},
    {{0x9A, 0x5F, 0xE3, 0xA5, 0x7C, 0x91, 0x89, 0x00}, [] { proto::SensorHeartbeat m{}; m.crc = 154; m.counter = 15; m.node_state = static_cast<proto::NodeState>(5); m.uptime = 2440865251; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 34; m.error_flags = 0; return m; }(), {154.0, 15.0, 5.0, 2440865251.0, 1.0, 34.0, 0.0}},
    {{0x55, 0x45, 0x99, 0x92, 0xA9, 0x66, 0x74, 0x1D}, [] { proto::SensorHeartbeat m{}; m.crc = 85; m.counter = 5; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 1722389145; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 29; m.error_flags = 29; return m; }(), {85.0, 5.0, 4.0, 1722389145.0, 0.0, 29.0, 29.0}},
    {{0xE9, 0x57, 0xFE, 0x74, 0x52, 0x44, 0x9F, 0x4D}, [] { proto::SensorHeartbeat m{}; m.crc = 233; m.counter = 7; m.node_state = static_cast<proto::NodeState>(5); m.uptime = 1146254590; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 39; m.error_flags = 77; return m; }(), {233.0, 7.0, 5.0, 1146254590.0, 3.0, 39.0, 77.0}},
    {{0xA6, 0x70, 0x70, 0x59, 0x3E, 0x32, 0x1B, 0x89}, [] { proto::SensorHeartbeat m{}; m.crc = 166; m.counter = 0; m.node_state = static_cast<proto::NodeState>(7); m.uptime = 842946928; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 6; m.error_flags = 137; return m; }(), {166.0, 0.0, 7.0, 842946928.0, 3.0, 6.0, 137.0}},
    {{0x24, 0xF1, 0x51, 0xD9, 0xB3, 0x00, 0x1D, 0x66}, [] { proto::SensorHeartbeat m{}; m.crc = 36; m.counter = 1; m.node_state = static_cast<proto::NodeState>(15); m.uptime = 11786577; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 7; m.error_flags = 102; return m; }(), {36.0, 1.0, 15.0, 11786577.0, 1.0, 7.0, 102.0}},
    {{0xFD, 0x3D, 0xB9, 0xBE, 0x85, 0xBB, 0x86, 0x44}, [] { proto::SensorHeartbeat m{}; m.crc = 253; m.counter = 13; m.node_state = static_cast<proto::NodeState>(3); m.uptime = 3146104505; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 33; m.error_flags = 68; return m; }(), {253.0, 13.0, 3.0, 3146104505.0, 2.0, 33.0, 68.0}},
    {{0xF5, 0x0C, 0x51, 0xFB, 0x95, 0x75, 0x85, 0x5E}, [] { proto::SensorHeartbeat m{}; m.crc = 245; m.counter = 12; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 1972763473; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 33; m.error_flags = 94; return m; }(), {245.0, 12.0, 0.0, 1972763473.0, 1.0, 33.0, 94.0}},
    {{0x3F, 0xD3, 0xAD, 0x7E, 0x01, 0xAC, 0x4C, 0x3C}, [] { proto::SensorHeartbeat m{}; m.crc = 63; m.counter = 3; m.node_state = static_cast<proto::NodeState>(13); m.uptime = 2885779117; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 19; m.error_flags = 60; return m; }(), {63.0, 3.0, 13.0, 2885779117.0, 0.0, 19.0, 60.0}},
    {{0x58, 0x9C, 0x73, 0x4B, 0xFD, 0x3E, 0x29, 0xB8}, [] { proto::SensorHeartbeat m{}; m.crc = 88; m.counter = 12; m.node_state = static_cast<proto::NodeState>(9); m.uptime = 1056787315; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 10; m.error_flags = 184; return m; }(), {88.0, 12.0, 9.0, 1056787315.0, 1.0, 10.0, 184.0}},
    {{0x9F, 0xB4, 0xC3, 0xEE, 0x0D, 0x26, 0x4C, 0x7F}, [] { proto::SensorHeartbeat m{}; m.crc = 159; m.counter = 4; m.node_state = static_cast<proto::NodeState>(11); m.uptime = 638447299; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 19; m.error_flags = 127; return m; }(), {159.0, 4.0, 11.0, 638447299.0, 0.0, 19.0, 127.0}},
    {{0x1B, 0xEF, 0xDE, 0x14, 0xBB, 0xC6, 0x4D, 0x6E}, [] { proto::SensorHeartbeat m{}; m.crc = 27; m.counter = 15; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 3334149342; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 19; m.error_flags = 110; return m; }(), {27.0, 15.0, 14.0, 3334149342.0, 1.0, 19.0, 110.0}},
    {{0xE2, 0xF9, 0x15, 0x77, 0x05, 0x6E, 0x94, 0x82}, [] { proto::SensorHeartbeat m{}; m.crc = 226; m.counter = 9; m.node_state = static_cast<proto::NodeState>(15); m.uptime = 1845851925; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 37; m.error_flags = 130; return m; }(), {226.0, 9.0, 15.0, 1845851925.0, 0.0, 37.0, 130.0}},
}};

/// Index of each signal of CONTROLLER_HEARTBEAT in the phys array (DBC order).
struct ControllerHeartbeatIdx {
    static constexpr std::size_t crc = 0;
    static constexpr std::size_t counter = 1;
    static constexpr std::size_t node_state = 2;
    static constexpr std::size_t uptime = 3;
    static constexpr std::size_t can_state = 4;
    static constexpr std::size_t bus_off_count = 5;
    static constexpr std::size_t error_flags = 6;
};

struct ControllerHeartbeatVector {
    proto::ControllerHeartbeat::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::ControllerHeartbeat expected;            ///< the raw values that were encoded
    std::array<double, 7> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<ControllerHeartbeatVector, 32> kControllerHeartbeat = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::ControllerHeartbeat m{}; m.crc = 0; m.counter = 0; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 0; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 0; m.error_flags = 0; return m; }(), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::ControllerHeartbeat m{}; m.crc = 255; m.counter = 15; m.node_state = static_cast<proto::NodeState>(15); m.uptime = 4294967295; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 63; m.error_flags = 255; return m; }(), {255.0, 15.0, 15.0, 4294967295.0, 3.0, 63.0, 255.0}},
    {{0x2A, 0x40, 0x5E, 0xFB, 0x80, 0x06, 0x0E, 0x47}, [] { proto::ControllerHeartbeat m{}; m.crc = 42; m.counter = 0; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 109116254; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 3; m.error_flags = 71; return m; }(), {42.0, 0.0, 4.0, 109116254.0, 2.0, 3.0, 71.0}},
    {{0xA8, 0xB5, 0x47, 0x94, 0x35, 0x40, 0xAD, 0xD6}, [] { proto::ControllerHeartbeat m{}; m.crc = 168; m.counter = 5; m.node_state = static_cast<proto::NodeState>(11); m.uptime = 1077253191; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 43; m.error_flags = 214; return m; }(), {168.0, 5.0, 11.0, 1077253191.0, 1.0, 43.0, 214.0}},
    {{0x32, 0x62, 0x04, 0x39, 0x3C, 0xA7, 0x18, 0x5E}, [] { proto::ControllerHeartbeat m{}; m.crc = 50; m.counter = 2; m.node_state = static_cast<proto::NodeState>(6); m.uptime = 2805741828; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 6; m.error_flags = 94; return m; }(), {50.0, 2.0, 6.0, 2805741828.0, 0.0, 6.0, 94.0}},
    {{0xA9, 0xEC, 0x65, 0x58, 0xB4, 0x73, 0x5C, 0xDE}, [] { proto::ControllerHeartbeat m{}; m.crc = 169; m.counter = 12; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 1941198949; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 23; m.error_flags = 222; return m; }(), {169.0, 12.0, 14.0, 1941198949.0, 0.0, 23.0, 222.0}},
    {{0xC2, 0x43, 0xD3, 0xD8, 0x6B, 0x6E, 0x43, 0x51}, [] { proto::ControllerHeartbeat m{}; m.crc = 194; m.counter = 3; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 1852561619; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 16; m.error_flags = 81; return m; }(), {194.0, 3.0, 4.0, 1852561619.0, 3.0, 16.0, 81.0}},
    {{0x0E, 0x90, 0xCC, 0xB6, 0x3C, 0xEF, 0x02, 0x92}, [] { proto::ControllerHeartbeat m{}; m.crc = 14; m.counter = 0; m.node_state = static_cast<proto::NodeState>(9); m.uptime = 4013733580; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 0; m.error_flags = 146; return m; }(), {14.0, 0.0, 9.0, 4013733580.0, 2.0, 0.0, 146.0}},
    {{0xAB, 0x36, 0x6C, 0xFA, 0x1D, 0x67, 0x49, 0x5F}, [] { proto::ControllerHeartbeat m{}; m.crc = 171; m.counter = 6; m.node_state = static_cast<proto::NodeState>(3); m.uptime = 1730017900; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 18; m.error_flags = 95; return m; }(), {171.0, 6.0, 3.0, 1730017900.0, 1.0, 18.0, 95.0}},
    {{0x41, 0x7B, 0x0C, 0x46, 0x32, 0x4F, 0xCF, 0xDE}, [] { proto::ControllerHeartbeat m{}; m.crc = 65; m.counter = 11; m.node_state = static_cast<proto::NodeState>(7); m.uptime = 1328694796; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 51; m.error_flags = 222; return m; }(), {65.0, 11.0, 7.0, 1328694796.0, 3.0, 51.0, 222.0}},
    {{0x40, 0x2C, 0x8E, 0x7A, 0xA8, 0x99, 0x50, 0xF5}, [] { proto::ControllerHeartbeat m{}; m.crc = 64; m.counter = 12; m.node_state = static_cast<proto::NodeState>(2); m.uptime = 2577955470; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 20; m.error_flags = 245; return m; }(), {64.0, 12.0, 2.0, 2577955470.0, 0.0, 20.0, 245.0}},
    {{0x4A, 0x51, 0x95, 0x0C, 0x01, 0x20, 0x2F, 0xFE}, [] { proto::ControllerHeartbeat m{}; m.crc = 74; m.counter = 1; m.node_state = static_cast<proto::NodeState>(5); m.uptime = 536939669; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 11; m.error_flags = 254; return m; }(), {74.0, 1.0, 5.0, 536939669.0, 3.0, 11.0, 254.0}},
    {{0xD0, 0x1E, 0xE0, 0x8E, 0xA6, 0x23, 0x1C, 0x6B}, [] { proto::ControllerHeartbeat m{}; m.crc = 208; m.counter = 14; m.node_state = static_cast<proto::NodeState>(1); m.uptime = 598118112; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 7; m.error_flags = 107; return m; }(), {208.0, 14.0, 1.0, 598118112.0, 0.0, 7.0, 107.0}},
    {{0x55, 0x12, 0x36, 0x4D, 0xCF, 0xDC, 0x5A, 0xDB}, [] { proto::ControllerHeartbeat m{}; m.crc = 85; m.counter = 2; m.node_state = static_cast<proto::NodeState>(1); m.uptime = 3704573238; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 22; m.error_flags = 219; return m; }(), {85.0, 2.0, 1.0, 3704573238.0, 2.0, 22.0, 219.0}},
    {{0xCD, 0xC4, 0x46, 0x0C, 0x80, 0x11, 0xAB, 0x1D}, [] { proto::ControllerHeartbeat m{}; m.crc = 205; m.counter = 4; m.node_state = static_cast<proto::NodeState>(12); m.uptime = 293604422; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 42; m.error_flags = 29; return m; }(), {205.0, 4.0, 12.0, 293604422.0, 3.0, 42.0, 29.0}},
    {{0xC0, 0x4E, 0x0E, 0xA1, 0xA3, 0xC8, 0x92, 0x75}, [] { proto::ControllerHeartbeat m{}; m.crc = 192; m.counter = 14; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 3366166798; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 36; m.error_flags = 117; return m; }(), {192.0, 14.0, 4.0, 3366166798.0, 2.0, 36.0, 117.0}},
    {{0x0C, 0x27, 0x8F, 0x71, 0x80, 0x05, 0xF1, 0x6E}, [] { proto::ControllerHeartbeat m{}; m.crc = 12; m.counter = 7; m.node_state = static_cast<proto::NodeState>(2); m.uptime = 92303759; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 60; m.error_flags = 110; return m; }(), {12.0, 7.0, 2.0, 92303759.0, 1.0, 60.0, 110.0}},
    {{0xDD, 0x1B, 0x5A, 0x04, 0x64, 0xCD, 0x08, 0x59}, [] { proto::ControllerHeartbeat m{}; m.crc = 221; m.counter = 11; m.node_state = static_cast<proto::NodeState>(1); m.uptime = 3445883994; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 2; m.error_flags = 89; return m; }(), {221.0, 11.0, 1.0, 3445883994.0, 0.0, 2.0, 89.0}},
    {{0xFF, 0x0A, 0x15, 0xC1, 0x4B, 0xA9, 0x49, 0xD4}, [] { proto::ControllerHeartbeat m{}; m.crc = 255; m.counter = 10; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 2840314133; m.can_state = static_cast<proto::CanState>(1); m.bus_off_count = 18; m.error_flags = 212; return m; }(), {255.0, 10.0, 0.0, 2840314133.0, 1.0, 18.0, 212.0}},
    {{0xF5, 0x41, 0x9B, 0x0F, 0x2E, 0xE0, 0xEA, 0x24}, [] { proto::ControllerHeartbeat m{}; m.crc = 245; m.counter = 1; m.node_state = static_cast<proto::NodeState>(4); m.uptime = 3761115035; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 58; m.error_flags = 36; return m; }(), {245.0, 1.0, 4.0, 3761115035.0, 2.0, 58.0, 36.0}},
    {{0xFA, 0xFA, 0xC6, 0x9C, 0xD6, 0xB0, 0x8B, 0x8E}, [] { proto::ControllerHeartbeat m{}; m.crc = 250; m.counter = 10; m.node_state = static_cast<proto::NodeState>(15); m.uptime = 2966854854; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 34; m.error_flags = 142; return m; }(), {250.0, 10.0, 15.0, 2966854854.0, 3.0, 34.0, 142.0}},
    {{0x03, 0xAF, 0xE8, 0x5E, 0x3A, 0x98, 0xD0, 0x16}, [] { proto::ControllerHeartbeat m{}; m.crc = 3; m.counter = 15; m.node_state = static_cast<proto::NodeState>(10); m.uptime = 2553962216; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 52; m.error_flags = 22; return m; }(), {3.0, 15.0, 10.0, 2553962216.0, 0.0, 52.0, 22.0}},
    {{0x30, 0xC6, 0xC8, 0x6F, 0xD6, 0x02, 0x32, 0x9D}, [] { proto::ControllerHeartbeat m{}; m.crc = 48; m.counter = 6; m.node_state = static_cast<proto::NodeState>(12); m.uptime = 47607752; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 12; m.error_flags = 157; return m; }(), {48.0, 6.0, 12.0, 47607752.0, 2.0, 12.0, 157.0}},
    {{0x8A, 0xD0, 0x11, 0x31, 0x00, 0xE2, 0xD3, 0x91}, [] { proto::ControllerHeartbeat m{}; m.crc = 138; m.counter = 0; m.node_state = static_cast<proto::NodeState>(13); m.uptime = 3791663377; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 52; m.error_flags = 145; return m; }(), {138.0, 0.0, 13.0, 3791663377.0, 3.0, 52.0, 145.0}},
    {{0x20, 0xE9, 0x3D, 0xA8, 0xAD, 0xB9, 0x0A, 0x90}, [] { proto::ControllerHeartbeat m{}; m.crc = 32; m.counter = 9; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 3115165757; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 2; m.error_flags = 144; return m; }(), {32.0, 9.0, 14.0, 3115165757.0, 2.0, 2.0, 144.0}},
    {{0x1E, 0x74, 0x84, 0xA5, 0x31, 0xB3, 0xD7, 0x21}, [] { proto::ControllerHeartbeat m{}; m.crc = 30; m.counter = 4; m.node_state = static_cast<proto::NodeState>(7); m.uptime = 3006375300; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 53; m.error_flags = 33; return m; }(), {30.0, 4.0, 7.0, 3006375300.0, 3.0, 53.0, 33.0}},
    {{0x7C, 0x34, 0xB4, 0xF0, 0x12, 0x04, 0x38, 0x6B}, [] { proto::ControllerHeartbeat m{}; m.crc = 124; m.counter = 4; m.node_state = static_cast<proto::NodeState>(3); m.uptime = 68350132; m.can_state = static_cast<proto::CanState>(0); m.bus_off_count = 14; m.error_flags = 107; return m; }(), {124.0, 4.0, 3.0, 68350132.0, 0.0, 14.0, 107.0}},
    {{0x25, 0x20, 0xC0, 0x05, 0xD5, 0xB3, 0xB6, 0x33}, [] { proto::ControllerHeartbeat m{}; m.crc = 37; m.counter = 0; m.node_state = static_cast<proto::NodeState>(2); m.uptime = 3017082304; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 45; m.error_flags = 51; return m; }(), {37.0, 0.0, 2.0, 3017082304.0, 2.0, 45.0, 51.0}},
    {{0x7B, 0x60, 0x0E, 0x2D, 0x97, 0x4E, 0x6F, 0x63}, [] { proto::ControllerHeartbeat m{}; m.crc = 123; m.counter = 0; m.node_state = static_cast<proto::NodeState>(6); m.uptime = 1318530318; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 27; m.error_flags = 99; return m; }(), {123.0, 0.0, 6.0, 1318530318.0, 3.0, 27.0, 99.0}},
    {{0x1C, 0xED, 0xBB, 0x7A, 0x62, 0xE7, 0x0E, 0x97}, [] { proto::ControllerHeartbeat m{}; m.crc = 28; m.counter = 13; m.node_state = static_cast<proto::NodeState>(14); m.uptime = 3881990843; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 3; m.error_flags = 151; return m; }(), {28.0, 13.0, 14.0, 3881990843.0, 2.0, 3.0, 151.0}},
    {{0x78, 0xA7, 0xD6, 0xF4, 0x68, 0x70, 0xCB, 0x66}, [] { proto::ControllerHeartbeat m{}; m.crc = 120; m.counter = 7; m.node_state = static_cast<proto::NodeState>(10); m.uptime = 1885926614; m.can_state = static_cast<proto::CanState>(3); m.bus_off_count = 50; m.error_flags = 102; return m; }(), {120.0, 7.0, 10.0, 1885926614.0, 3.0, 50.0, 102.0}},
    {{0xBC, 0x09, 0x6E, 0xB6, 0x68, 0x57, 0x4E, 0x91}, [] { proto::ControllerHeartbeat m{}; m.crc = 188; m.counter = 9; m.node_state = static_cast<proto::NodeState>(0); m.uptime = 1466480238; m.can_state = static_cast<proto::CanState>(2); m.bus_off_count = 19; m.error_flags = 145; return m; }(), {188.0, 9.0, 0.0, 1466480238.0, 2.0, 19.0, 145.0}},
}};

/// Index of each signal of ECHO_REQUEST in the phys array (DBC order).
struct EchoRequestIdx {
    static constexpr std::size_t target_node = 0;
    static constexpr std::size_t seq = 1;
    static constexpr std::size_t payload = 2;
};

struct EchoRequestVector {
    proto::EchoRequest::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::EchoRequest expected;            ///< the raw values that were encoded
    std::array<double, 3> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<EchoRequestVector, 32> kEchoRequest = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(0); m.seq = 0; m.payload = 0; return m; }(), {0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(255); m.seq = 255; m.payload = 4294967295; return m; }(), {255.0, 255.0, 4294967295.0}},
    {{0xCB, 0x7D, 0x3E, 0x08, 0x50, 0xE9}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(203); m.seq = 125; m.payload = 3914336318; return m; }(), {203.0, 125.0, 3914336318.0}},
    {{0x98, 0x73, 0x3D, 0xD7, 0xDD, 0xE5}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(152); m.seq = 115; m.payload = 3856521021; return m; }(), {152.0, 115.0, 3856521021.0}},
    {{0xEB, 0x91, 0x1F, 0x04, 0x3E, 0x6F}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(235); m.seq = 145; m.payload = 1866335263; return m; }(), {235.0, 145.0, 1866335263.0}},
    {{0xD2, 0xA7, 0x73, 0x43, 0x7D, 0x7E}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(210); m.seq = 167; m.payload = 2122138483; return m; }(), {210.0, 167.0, 2122138483.0}},
    {{0x51, 0x60, 0xD9, 0xC1, 0xF0, 0x8A}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(81); m.seq = 96; m.payload = 2331034073; return m; }(), {81.0, 96.0, 2331034073.0}},
    {{0x7D, 0x19, 0x5E, 0xC8, 0x34, 0x91}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(125); m.seq = 25; m.payload = 2436155486; return m; }(), {125.0, 25.0, 2436155486.0}},
    {{0xF5, 0xAB, 0xA2, 0x26, 0xA9, 0x9D}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(245); m.seq = 171; m.payload = 2645108386; return m; }(), {245.0, 171.0, 2645108386.0}},
    {{0x57, 0x0A, 0x88, 0x5C, 0x3F, 0x77}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(87); m.seq = 10; m.payload = 2000641160; return m; }(), {87.0, 10.0, 2000641160.0}},
    {{0x7A, 0xCD, 0x76, 0x78, 0xC8, 0x90}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(122); m.seq = 205; m.payload = 2429057142; return m; }(), {122.0, 205.0, 2429057142.0}},
    {{0x14, 0xD0, 0x5B, 0x1E, 0x72, 0x58}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(20); m.seq = 208; m.payload = 1483873883; return m; }(), {20.0, 208.0, 1483873883.0}},
    {{0x8B, 0x8E, 0xEB, 0x84, 0x5F, 0x1B}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(139); m.seq = 142; m.payload = 459244779; return m; }(), {139.0, 142.0, 459244779.0}},
    {{0xD3, 0xAB, 0x47, 0x8F, 0xAF, 0xDD}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(211); m.seq = 171; m.payload = 3719270215; return m; }(), {211.0, 171.0, 3719270215.0}},
    {{0x93, 0xEF, 0x7F, 0x5F, 0xBC, 0xC2}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(147); m.seq = 239; m.payload = 3267125119; return m; }(), {147.0, 239.0, 3267125119.0}},
    {{0x49, 0x21, 0x1B, 0xBA, 0xE9, 0x39}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(73); m.seq = 33; m.payload = 971618843; return m; }(), {73.0, 33.0, 971618843.0}},
    {{0x6E, 0xB1, 0x5A, 0xF8, 0x64, 0x6D}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(110); m.seq = 177; m.payload = 1835333722; return m; }(), {110.0, 177.0, 1835333722.0}},
    {{0x46, 0x14, 0x23, 0x8B, 0x29, 0x5C}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(70); m.seq = 20; m.payload = 1546226467; return m; }(), {70.0, 20.0, 1546226467.0}},
    {{0x03, 0xB7, 0xA0, 0x83, 0x0F, 0xCD}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(3); m.seq = 183; m.payload = 3440346016; return m; }(), {3.0, 183.0, 3440346016.0}},
    {{0x74, 0xA6, 0xEE, 0x14, 0xC5, 0x74}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(116); m.seq = 166; m.payload = 1959073006; return m; }(), {116.0, 166.0, 1959073006.0}},
    {{0x8A, 0x42, 0x8D, 0x76, 0xDE, 0xBC}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(138); m.seq = 66; m.payload = 3168695949; return m; }(), {138.0, 66.0, 3168695949.0}},
    {{0x3B, 0x83, 0xDD, 0x2A, 0xDE, 0xC6}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(59); m.seq = 131; m.payload = 3336448733; return m; }(), {59.0, 131.0, 3336448733.0}},
    {{0xCA, 0xE2, 0x1E, 0xBB, 0x57, 0x24}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(202); m.seq = 226; m.payload = 609729310; return m; }(), {202.0, 226.0, 609729310.0}},
    {{0xB7, 0x30, 0xA2, 0x50, 0x0F, 0x03}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(183); m.seq = 48; m.payload = 51335330; return m; }(), {183.0, 48.0, 51335330.0}},
    {{0x5A, 0xDC, 0x11, 0xE0, 0xC0, 0x04}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(90); m.seq = 220; m.payload = 79749137; return m; }(), {90.0, 220.0, 79749137.0}},
    {{0x76, 0x97, 0xE0, 0xBC, 0xFE, 0x18}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(118); m.seq = 151; m.payload = 419347680; return m; }(), {118.0, 151.0, 419347680.0}},
    {{0xDD, 0x6E, 0x23, 0x8F, 0x4C, 0x2E}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(221); m.seq = 110; m.payload = 776769315; return m; }(), {221.0, 110.0, 776769315.0}},
    {{0x3A, 0x58, 0x48, 0x6B, 0x06, 0xAD}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(58); m.seq = 88; m.payload = 2902879048; return m; }(), {58.0, 88.0, 2902879048.0}},
    {{0x78, 0x43, 0x40, 0x2C, 0xDE, 0xA2}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(120); m.seq = 67; m.payload = 2732469312; return m; }(), {120.0, 67.0, 2732469312.0}},
    {{0x07, 0xFE, 0xD5, 0xDA, 0xC2, 0xAD}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(7); m.seq = 254; m.payload = 2915228373; return m; }(), {7.0, 254.0, 2915228373.0}},
    {{0x84, 0x74, 0xCD, 0x65, 0x0E, 0xFB}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(132); m.seq = 116; m.payload = 4212024781; return m; }(), {132.0, 116.0, 4212024781.0}},
    {{0xFD, 0xC4, 0x60, 0x85, 0x88, 0xA7}, [] { proto::EchoRequest m{}; m.target_node = static_cast<proto::Node>(253); m.seq = 196; m.payload = 2810742112; return m; }(), {253.0, 196.0, 2810742112.0}},
}};

/// Index of each signal of SENSOR_ECHO_RESPONSE in the phys array (DBC order).
struct SensorEchoResponseIdx {
    static constexpr std::size_t seq = 0;
    static constexpr std::size_t payload = 1;
    static constexpr std::size_t turnaround = 2;
};

struct SensorEchoResponseVector {
    proto::SensorEchoResponse::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::SensorEchoResponse expected;            ///< the raw values that were encoded
    std::array<double, 3> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<SensorEchoResponseVector, 32> kSensorEchoResponse = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::SensorEchoResponse m{}; m.seq = 0; m.payload = 0; m.turnaround = 0; return m; }(), {0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::SensorEchoResponse m{}; m.seq = 255; m.payload = 4294967295; m.turnaround = 65535; return m; }(), {255.0, 4294967295.0, 65535.0}},
    {{0x51, 0x2C, 0xED, 0x60, 0xEF, 0x91, 0xE1}, [] { proto::SensorEchoResponse m{}; m.seq = 81; m.payload = 4016106796; m.turnaround = 57745; return m; }(), {81.0, 4016106796.0, 57745.0}},
    {{0x5A, 0x1C, 0xAE, 0x95, 0x27, 0x18, 0x55}, [] { proto::SensorEchoResponse m{}; m.seq = 90; m.payload = 664120860; m.turnaround = 21784; return m; }(), {90.0, 664120860.0, 21784.0}},
    {{0xCC, 0x9A, 0xFD, 0xAB, 0x77, 0x3D, 0x8C}, [] { proto::SensorEchoResponse m{}; m.seq = 204; m.payload = 2007760282; m.turnaround = 35901; return m; }(), {204.0, 2007760282.0, 35901.0}},
    {{0x43, 0xB3, 0x8B, 0x45, 0xE5, 0xA5, 0x7A}, [] { proto::SensorEchoResponse m{}; m.seq = 67; m.payload = 3846540211; m.turnaround = 31397; return m; }(), {67.0, 3846540211.0, 31397.0}},
    {{0x69, 0x2B, 0x3A, 0x47, 0x09, 0xDD, 0x5C}, [] { proto::SensorEchoResponse m{}; m.seq = 105; m.payload = 155662891; m.turnaround = 23773; return m; }(), {105.0, 155662891.0, 23773.0}},
    {{0x7B, 0x0C, 0x9B, 0x75, 0xB2, 0x55, 0xB8}, [] { proto::SensorEchoResponse m{}; m.seq = 123; m.payload = 2994051852; m.turnaround = 47189; return m; }(), {123.0, 2994051852.0, 47189.0}},
    {{0x97, 0xB1, 0xF8, 0xE2, 0xA8, 0x54, 0xEE}, [] { proto::SensorEchoResponse m{}; m.seq = 151; m.payload = 2833447089; m.turnaround = 61012; return m; }(), {151.0, 2833447089.0, 61012.0}},
    {{0xDD, 0x92, 0xF6, 0x6F, 0x3E, 0xB1, 0xED}, [] { proto::SensorEchoResponse m{}; m.seq = 221; m.payload = 1047525010; m.turnaround = 60849; return m; }(), {221.0, 1047525010.0, 60849.0}},
    {{0x31, 0x8A, 0x32, 0xBE, 0x7E, 0x58, 0x9A}, [] { proto::SensorEchoResponse m{}; m.seq = 49; m.payload = 2126393994; m.turnaround = 39512; return m; }(), {49.0, 2126393994.0, 39512.0}},
    {{0xC3, 0x88, 0xDD, 0x46, 0x5A, 0xB8, 0xDD}, [] { proto::SensorEchoResponse m{}; m.seq = 195; m.payload = 1514593672; m.turnaround = 56760; return m; }(), {195.0, 1514593672.0, 56760.0}},
    {{0x0F, 0x97, 0xC5, 0x77, 0xCA, 0x77, 0x56}, [] { proto::SensorEchoResponse m{}; m.seq = 15; m.payload = 3396846999; m.turnaround = 22135; return m; }(), {15.0, 3396846999.0, 22135.0}},
    {{0xED, 0x6C, 0xE6, 0xB8, 0x8F, 0x3A, 0xCF}, [] { proto::SensorEchoResponse m{}; m.seq = 237; m.payload = 2411259500; m.turnaround = 53050; return m; }(), {237.0, 2411259500.0, 53050.0}},
    {{0xDD, 0xF7, 0x26, 0xC4, 0x61, 0x71, 0xFE}, [] { proto::SensorEchoResponse m{}; m.seq = 221; m.payload = 1640244983; m.turnaround = 65137; return m; }(), {221.0, 1640244983.0, 65137.0}},
    {{0x21, 0x36, 0x13, 0x08, 0x91, 0x53, 0x85}, [] { proto::SensorEchoResponse m{}; m.seq = 33; m.payload = 2433225526; m.turnaround = 34131; return m; }(), {33.0, 2433225526.0, 34131.0}},
    {{0x00, 0xB5, 0x14, 0xA1, 0x4E, 0x80, 0xF4}, [] { proto::SensorEchoResponse m{}; m.seq = 0; m.payload = 1319179445; m.turnaround = 62592; return m; }(), {0.0, 1319179445.0, 62592.0}},
    {{0x22, 0x7F, 0x4D, 0x80, 0x12, 0x30, 0x41}, [] { proto::SensorEchoResponse m{}; m.seq = 34; m.payload = 310398335; m.turnaround = 16688; return m; }(), {34.0, 310398335.0, 16688.0}},
    {{0xD0, 0x5A, 0x46, 0xFE, 0x05, 0x70, 0x27}, [] { proto::SensorEchoResponse m{}; m.seq = 208; m.payload = 100550234; m.turnaround = 10096; return m; }(), {208.0, 100550234.0, 10096.0}},
    {{0x95, 0x8C, 0xCB, 0x68, 0xE3, 0x85, 0xC5}, [] { proto::SensorEchoResponse m{}; m.seq = 149; m.payload = 3815295884; m.turnaround = 50565; return m; }(), {149.0, 3815295884.0, 50565.0}},
    {{0x89, 0xFE, 0xCE, 0x2E, 0x6D, 0x28, 0xD0}, [] { proto::SensorEchoResponse m{}; m.seq = 137; m.payload = 1831784190; m.turnaround = 53288; return m; }(), {137.0, 1831784190.0, 53288.0}},
    {{0x84, 0x76, 0xA5, 0x22, 0x4E, 0xCA, 0xC9}, [] { proto::SensorEchoResponse m{}; m.seq = 132; m.payload = 1310893430; m.turnaround = 51658; return m; }(), {132.0, 1310893430.0, 51658.0}},
    {{0xD0, 0x3E, 0x64, 0xA8, 0xC8, 0xF6, 0xEE}, [] { proto::SensorEchoResponse m{}; m.seq = 208; m.payload = 3366478910; m.turnaround = 61174; return m; }(), {208.0, 3366478910.0, 61174.0}},
    {{0xB3, 0xB9, 0xF2, 0xDF, 0xD2, 0x17, 0x40}, [] { proto::SensorEchoResponse m{}; m.seq = 179; m.payload = 3537892025; m.turnaround = 16407; return m; }(), {179.0, 3537892025.0, 16407.0}},
    {{0xFC, 0x1B, 0xAD, 0x2C, 0xD9, 0x65, 0xB8}, [] { proto::SensorEchoResponse m{}; m.seq = 252; m.payload = 3643583771; m.turnaround = 47205; return m; }(), {252.0, 3643583771.0, 47205.0}},
    {{0xA1, 0xF7, 0x1D, 0xB5, 0xED, 0xC2, 0x6C}, [] { proto::SensorEchoResponse m{}; m.seq = 161; m.payload = 3988069879; m.turnaround = 27842; return m; }(), {161.0, 3988069879.0, 27842.0}},
    {{0x40, 0x15, 0x82, 0x06, 0x2B, 0xF0, 0x08}, [] { proto::SensorEchoResponse m{}; m.seq = 64; m.payload = 721846805; m.turnaround = 2288; return m; }(), {64.0, 721846805.0, 2288.0}},
    {{0x85, 0x54, 0x78, 0x7F, 0x40, 0x5F, 0x69}, [] { proto::SensorEchoResponse m{}; m.seq = 133; m.payload = 1082095700; m.turnaround = 26975; return m; }(), {133.0, 1082095700.0, 26975.0}},
    {{0xF6, 0x30, 0xE3, 0xAA, 0xCB, 0xA4, 0x5C}, [] { proto::SensorEchoResponse m{}; m.seq = 246; m.payload = 3416974128; m.turnaround = 23716; return m; }(), {246.0, 3416974128.0, 23716.0}},
    {{0xAE, 0x22, 0x52, 0x62, 0xBF, 0x4B, 0x90}, [] { proto::SensorEchoResponse m{}; m.seq = 174; m.payload = 3210891810; m.turnaround = 36939; return m; }(), {174.0, 3210891810.0, 36939.0}},
    {{0x9A, 0x09, 0x7C, 0x70, 0x5B, 0x20, 0xEC}, [] { proto::SensorEchoResponse m{}; m.seq = 154; m.payload = 1534098441; m.turnaround = 60448; return m; }(), {154.0, 1534098441.0, 60448.0}},
    {{0xC1, 0xFE, 0x06, 0x1D, 0xB3, 0x68, 0x98}, [] { proto::SensorEchoResponse m{}; m.seq = 193; m.payload = 3005023998; m.turnaround = 39016; return m; }(), {193.0, 3005023998.0, 39016.0}},
}};

/// Index of each signal of CONTROLLER_ECHO_RESPONSE in the phys array (DBC order).
struct ControllerEchoResponseIdx {
    static constexpr std::size_t seq = 0;
    static constexpr std::size_t payload = 1;
    static constexpr std::size_t turnaround = 2;
};

struct ControllerEchoResponseVector {
    proto::ControllerEchoResponse::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::ControllerEchoResponse expected;            ///< the raw values that were encoded
    std::array<double, 3> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<ControllerEchoResponseVector, 32> kControllerEchoResponse = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::ControllerEchoResponse m{}; m.seq = 0; m.payload = 0; m.turnaround = 0; return m; }(), {0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::ControllerEchoResponse m{}; m.seq = 255; m.payload = 4294967295; m.turnaround = 65535; return m; }(), {255.0, 4294967295.0, 65535.0}},
    {{0x10, 0x08, 0x6F, 0x31, 0x40, 0x4A, 0x52}, [] { proto::ControllerEchoResponse m{}; m.seq = 16; m.payload = 1076981512; m.turnaround = 21066; return m; }(), {16.0, 1076981512.0, 21066.0}},
    {{0xC5, 0x25, 0x56, 0x87, 0x86, 0xA7, 0x06}, [] { proto::ControllerEchoResponse m{}; m.seq = 197; m.payload = 2257016357; m.turnaround = 1703; return m; }(), {197.0, 2257016357.0, 1703.0}},
    {{0xCB, 0x22, 0x49, 0xB5, 0xAF, 0xCD, 0x50}, [] { proto::ControllerEchoResponse m{}; m.seq = 203; m.payload = 2947893538; m.turnaround = 20685; return m; }(), {203.0, 2947893538.0, 20685.0}},
    {{0xEE, 0x80, 0x2A, 0x0F, 0x82, 0xD3, 0x82}, [] { proto::ControllerEchoResponse m{}; m.seq = 238; m.payload = 2182032000; m.turnaround = 33491; return m; }(), {238.0, 2182032000.0, 33491.0}},
    {{0xA4, 0xCA, 0xF7, 0xCD, 0x7C, 0x32, 0x2A}, [] { proto::ControllerEchoResponse m{}; m.seq = 164; m.payload = 2093873098; m.turnaround = 10802; return m; }(), {164.0, 2093873098.0, 10802.0}},
    {{0xD1, 0x00, 0x86, 0x59, 0x6E, 0xB9, 0x31}, [] { proto::ControllerEchoResponse m{}; m.seq = 209; m.payload = 1851360768; m.turnaround = 12729; return m; }(), {209.0, 1851360768.0, 12729.0}},
    {{0x39, 0xBC, 0x89, 0x27, 0x58, 0x50, 0xE6}, [] { proto::ControllerEchoResponse m{}; m.seq = 57; m.payload = 1478986172; m.turnaround = 58960; return m; }(), {57.0, 1478986172.0, 58960.0}},
    {{0x8D, 0x85, 0xC4, 0x4B, 0x7D, 0xAB, 0x64}, [] { proto::ControllerEchoResponse m{}; m.seq = 141; m.payload = 2102117509; m.turnaround = 25771; return m; }(), {141.0, 2102117509.0, 25771.0}},
    {{0x31, 0x9B, 0xE2, 0xF1, 0xFC, 0x9B, 0x02}, [] { proto::ControllerEchoResponse m{}; m.seq = 49; m.payload = 4243710619; m.turnaround = 667; return m; }(), {49.0, 4243710619.0, 667.0}},
    {{0xED, 0x3A, 0xAA, 0x73, 0x5D, 0xCF, 0x21}, [] { proto::ControllerEchoResponse m{}; m.seq = 237; m.payload = 1567861306; m.turnaround = 8655; return m; }(), {237.0, 1567861306.0, 8655.0}},
    {{0x1E, 0xF4, 0x3F, 0xF4, 0xD4, 0xC0, 0xB1}, [] { proto::ControllerEchoResponse m{}; m.seq = 30; m.payload = 3572776948; m.turnaround = 45504; return m; }(), {30.0, 3572776948.0, 45504.0}},
    {{0xCD, 0xEA, 0x6C, 0xFF, 0xB3, 0xFE, 0xB9}, [] { proto::ControllerEchoResponse m{}; m.seq = 205; m.payload = 3019861226; m.turnaround = 47614; return m; }(), {205.0, 3019861226.0, 47614.0}},
    {{0x16, 0x30, 0x86, 0xB0, 0x40, 0x35, 0xA6}, [] { proto::ControllerEchoResponse m{}; m.seq = 22; m.payload = 1085310512; m.turnaround = 42549; return m; }(), {22.0, 1085310512.0, 42549.0}},
    {{0xF4, 0xD3, 0xE4, 0x4C, 0xF6, 0xD3, 0xB3}, [] { proto::ControllerEchoResponse m{}; m.seq = 244; m.payload = 4132234451; m.turnaround = 46035; return m; }(), {244.0, 4132234451.0, 46035.0}},
    {{0xBC, 0x26, 0x75, 0xF2, 0x25, 0x34, 0xFD}, [] { proto::ControllerEchoResponse m{}; m.seq = 188; m.payload = 636646694; m.turnaround = 64820; return m; }(), {188.0, 636646694.0, 64820.0}},
    {{0x09, 0x41, 0x54, 0x34, 0x8F, 0xE5, 0xEE}, [] { proto::ControllerEchoResponse m{}; m.seq = 9; m.payload = 2402571329; m.turnaround = 61157; return m; }(), {9.0, 2402571329.0, 61157.0}},
    {{0x4E, 0xA7, 0x16, 0x0B, 0x18, 0x7E, 0x30}, [] { proto::ControllerEchoResponse m{}; m.seq = 78; m.payload = 403379879; m.turnaround = 12414; return m; }(), {78.0, 403379879.0, 12414.0}},
    {{0x43, 0x9C, 0x65, 0x76, 0x42, 0x7F, 0x1C}, [] { proto::ControllerEchoResponse m{}; m.seq = 67; m.payload = 1115055516; m.turnaround = 7295; return m; }(), {67.0, 1115055516.0, 7295.0}},
    {{0x87, 0x2C, 0x0C, 0x8C, 0x5D, 0x2D, 0x93}, [] { proto::ControllerEchoResponse m{}; m.seq = 135; m.payload = 1569459244; m.turnaround = 37677; return m; }(), {135.0, 1569459244.0, 37677.0}},
    {{0x3C, 0x47, 0x5D, 0xCC, 0x82, 0x5F, 0x2C}, [] { proto::ControllerEchoResponse m{}; m.seq = 60; m.payload = 2194431303; m.turnaround = 11359; return m; }(), {60.0, 2194431303.0, 11359.0}},
    {{0xBB, 0x02, 0xEA, 0x0A, 0xB0, 0x50, 0x19}, [] { proto::ControllerEchoResponse m{}; m.seq = 187; m.payload = 2953505282; m.turnaround = 6480; return m; }(), {187.0, 2953505282.0, 6480.0}},
    {{0xF1, 0x61, 0xDE, 0xD3, 0x7A, 0xC9, 0x12}, [] { proto::ControllerEchoResponse m{}; m.seq = 241; m.payload = 2060705377; m.turnaround = 4809; return m; }(), {241.0, 2060705377.0, 4809.0}},
    {{0x3E, 0xB6, 0xD3, 0x27, 0xAB, 0x69, 0x51}, [] { proto::ControllerEchoResponse m{}; m.seq = 62; m.payload = 2871514038; m.turnaround = 20841; return m; }(), {62.0, 2871514038.0, 20841.0}},
    {{0x68, 0x26, 0x8D, 0x3B, 0x7D, 0xF0, 0xCF}, [] { proto::ControllerEchoResponse m{}; m.seq = 104; m.payload = 2101054758; m.turnaround = 53232; return m; }(), {104.0, 2101054758.0, 53232.0}},
    {{0x4E, 0x54, 0xC7, 0x56, 0xF7, 0xF4, 0xE9}, [] { proto::ControllerEchoResponse m{}; m.seq = 78; m.payload = 4149659476; m.turnaround = 59892; return m; }(), {78.0, 4149659476.0, 59892.0}},
    {{0xDD, 0x1C, 0x0F, 0x9B, 0x01, 0x29, 0x65}, [] { proto::ControllerEchoResponse m{}; m.seq = 221; m.payload = 26939164; m.turnaround = 25897; return m; }(), {221.0, 26939164.0, 25897.0}},
    {{0xD7, 0xC6, 0x5D, 0xA9, 0x01, 0xA1, 0x20}, [] { proto::ControllerEchoResponse m{}; m.seq = 215; m.payload = 27876806; m.turnaround = 8353; return m; }(), {215.0, 27876806.0, 8353.0}},
    {{0x4A, 0x4B, 0x8C, 0xE0, 0x7B, 0x7D, 0x0B}, [] { proto::ControllerEchoResponse m{}; m.seq = 74; m.payload = 2078313547; m.turnaround = 2941; return m; }(), {74.0, 2078313547.0, 2941.0}},
    {{0xBC, 0x17, 0x03, 0x9E, 0x0E, 0x95, 0xEE}, [] { proto::ControllerEchoResponse m{}; m.seq = 188; m.payload = 245236503; m.turnaround = 61077; return m; }(), {188.0, 245236503.0, 61077.0}},
    {{0x90, 0xB7, 0x8F, 0x4F, 0xA1, 0xFF, 0x91}, [] { proto::ControllerEchoResponse m{}; m.seq = 144; m.payload = 2706345911; m.turnaround = 37375; return m; }(), {144.0, 2706345911.0, 37375.0}},
}};

/// Index of each signal of TEST_CONTROL in the phys array (DBC order).
struct TestControlIdx {
    static constexpr std::size_t magic = 0;
    static constexpr std::size_t target_node = 1;
    static constexpr std::size_t command = 2;
    static constexpr std::size_t arg0 = 3;
    static constexpr std::size_t arg1 = 4;
};

struct TestControlVector {
    proto::TestControl::Frame bytes{};      ///< encoded by cantools (raw values)
    proto::TestControl expected;            ///< the raw values that were encoded
    std::array<double, 5> phys{};  ///< cantools' scaled decode of bytes
};

inline const std::array<TestControlVector, 32> kTestControl = {{
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, [] { proto::TestControl m{}; m.magic = 0; m.target_node = static_cast<proto::Node>(0); m.command = static_cast<proto::TestCommand>(0); m.arg0 = 0; m.arg1 = 0; return m; }(), {0.0, 0.0, 0.0, 0.0, 0.0}},
    {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, [] { proto::TestControl m{}; m.magic = 65535; m.target_node = static_cast<proto::Node>(255); m.command = static_cast<proto::TestCommand>(255); m.arg0 = 65535; m.arg1 = 65535; return m; }(), {65535.0, 255.0, 255.0, 65535.0, 65535.0}},
    {{0x42, 0x46, 0x10, 0xF0, 0x1C, 0xCE, 0x91, 0x05}, [] { proto::TestControl m{}; m.magic = 17986; m.target_node = static_cast<proto::Node>(16); m.command = static_cast<proto::TestCommand>(240); m.arg0 = 52764; m.arg1 = 1425; return m; }(), {17986.0, 16.0, 240.0, 52764.0, 1425.0}},
    {{0xAA, 0x09, 0x13, 0xFC, 0x95, 0x82, 0xB8, 0x93}, [] { proto::TestControl m{}; m.magic = 2474; m.target_node = static_cast<proto::Node>(19); m.command = static_cast<proto::TestCommand>(252); m.arg0 = 33429; m.arg1 = 37816; return m; }(), {2474.0, 19.0, 252.0, 33429.0, 37816.0}},
    {{0x0B, 0xFC, 0xB0, 0x4C, 0x1C, 0xB3, 0x03, 0x4D}, [] { proto::TestControl m{}; m.magic = 64523; m.target_node = static_cast<proto::Node>(176); m.command = static_cast<proto::TestCommand>(76); m.arg0 = 45852; m.arg1 = 19715; return m; }(), {64523.0, 176.0, 76.0, 45852.0, 19715.0}},
    {{0xE6, 0x03, 0xBA, 0x4C, 0xCC, 0xAF, 0xD1, 0xFE}, [] { proto::TestControl m{}; m.magic = 998; m.target_node = static_cast<proto::Node>(186); m.command = static_cast<proto::TestCommand>(76); m.arg0 = 45004; m.arg1 = 65233; return m; }(), {998.0, 186.0, 76.0, 45004.0, 65233.0}},
    {{0x0C, 0x14, 0x32, 0x0C, 0x25, 0x99, 0xC8, 0xBB}, [] { proto::TestControl m{}; m.magic = 5132; m.target_node = static_cast<proto::Node>(50); m.command = static_cast<proto::TestCommand>(12); m.arg0 = 39205; m.arg1 = 48072; return m; }(), {5132.0, 50.0, 12.0, 39205.0, 48072.0}},
    {{0x7B, 0xE6, 0xF1, 0x08, 0x43, 0xC2, 0xC6, 0x9D}, [] { proto::TestControl m{}; m.magic = 59003; m.target_node = static_cast<proto::Node>(241); m.command = static_cast<proto::TestCommand>(8); m.arg0 = 49731; m.arg1 = 40390; return m; }(), {59003.0, 241.0, 8.0, 49731.0, 40390.0}},
    {{0x6E, 0x60, 0xCB, 0xAC, 0x0F, 0xC0, 0xC3, 0xE7}, [] { proto::TestControl m{}; m.magic = 24686; m.target_node = static_cast<proto::Node>(203); m.command = static_cast<proto::TestCommand>(172); m.arg0 = 49167; m.arg1 = 59331; return m; }(), {24686.0, 203.0, 172.0, 49167.0, 59331.0}},
    {{0xCF, 0x8E, 0xA3, 0x60, 0x66, 0xF9, 0x25, 0xF7}, [] { proto::TestControl m{}; m.magic = 36559; m.target_node = static_cast<proto::Node>(163); m.command = static_cast<proto::TestCommand>(96); m.arg0 = 63846; m.arg1 = 63269; return m; }(), {36559.0, 163.0, 96.0, 63846.0, 63269.0}},
    {{0xDF, 0xFE, 0x21, 0x7A, 0xC9, 0x08, 0x3E, 0x71}, [] { proto::TestControl m{}; m.magic = 65247; m.target_node = static_cast<proto::Node>(33); m.command = static_cast<proto::TestCommand>(122); m.arg0 = 2249; m.arg1 = 28990; return m; }(), {65247.0, 33.0, 122.0, 2249.0, 28990.0}},
    {{0x82, 0x3D, 0x93, 0x91, 0x74, 0x55, 0x66, 0x63}, [] { proto::TestControl m{}; m.magic = 15746; m.target_node = static_cast<proto::Node>(147); m.command = static_cast<proto::TestCommand>(145); m.arg0 = 21876; m.arg1 = 25446; return m; }(), {15746.0, 147.0, 145.0, 21876.0, 25446.0}},
    {{0x9A, 0x08, 0x70, 0xF5, 0x54, 0x7E, 0xD3, 0xB2}, [] { proto::TestControl m{}; m.magic = 2202; m.target_node = static_cast<proto::Node>(112); m.command = static_cast<proto::TestCommand>(245); m.arg0 = 32340; m.arg1 = 45779; return m; }(), {2202.0, 112.0, 245.0, 32340.0, 45779.0}},
    {{0x8D, 0x01, 0xB9, 0xB3, 0x3B, 0xA3, 0xFF, 0x42}, [] { proto::TestControl m{}; m.magic = 397; m.target_node = static_cast<proto::Node>(185); m.command = static_cast<proto::TestCommand>(179); m.arg0 = 41787; m.arg1 = 17151; return m; }(), {397.0, 185.0, 179.0, 41787.0, 17151.0}},
    {{0xE9, 0x39, 0x35, 0x18, 0x09, 0xF2, 0x18, 0xF6}, [] { proto::TestControl m{}; m.magic = 14825; m.target_node = static_cast<proto::Node>(53); m.command = static_cast<proto::TestCommand>(24); m.arg0 = 61961; m.arg1 = 63000; return m; }(), {14825.0, 53.0, 24.0, 61961.0, 63000.0}},
    {{0xC8, 0xC1, 0x1B, 0x91, 0x78, 0x2F, 0x5B, 0x54}, [] { proto::TestControl m{}; m.magic = 49608; m.target_node = static_cast<proto::Node>(27); m.command = static_cast<proto::TestCommand>(145); m.arg0 = 12152; m.arg1 = 21595; return m; }(), {49608.0, 27.0, 145.0, 12152.0, 21595.0}},
    {{0xA4, 0xE0, 0x7D, 0x9B, 0xC0, 0xF8, 0x13, 0x50}, [] { proto::TestControl m{}; m.magic = 57508; m.target_node = static_cast<proto::Node>(125); m.command = static_cast<proto::TestCommand>(155); m.arg0 = 63680; m.arg1 = 20499; return m; }(), {57508.0, 125.0, 155.0, 63680.0, 20499.0}},
    {{0x42, 0x79, 0x70, 0x1C, 0x9D, 0x89, 0x95, 0x57}, [] { proto::TestControl m{}; m.magic = 31042; m.target_node = static_cast<proto::Node>(112); m.command = static_cast<proto::TestCommand>(28); m.arg0 = 35229; m.arg1 = 22421; return m; }(), {31042.0, 112.0, 28.0, 35229.0, 22421.0}},
    {{0x2B, 0xA0, 0x55, 0x2E, 0x06, 0x90, 0x79, 0xD7}, [] { proto::TestControl m{}; m.magic = 41003; m.target_node = static_cast<proto::Node>(85); m.command = static_cast<proto::TestCommand>(46); m.arg0 = 36870; m.arg1 = 55161; return m; }(), {41003.0, 85.0, 46.0, 36870.0, 55161.0}},
    {{0x53, 0x67, 0x99, 0x2A, 0x4E, 0xB3, 0x31, 0x14}, [] { proto::TestControl m{}; m.magic = 26451; m.target_node = static_cast<proto::Node>(153); m.command = static_cast<proto::TestCommand>(42); m.arg0 = 45902; m.arg1 = 5169; return m; }(), {26451.0, 153.0, 42.0, 45902.0, 5169.0}},
    {{0x68, 0x17, 0xEB, 0x1E, 0x53, 0xA5, 0x2D, 0x81}, [] { proto::TestControl m{}; m.magic = 5992; m.target_node = static_cast<proto::Node>(235); m.command = static_cast<proto::TestCommand>(30); m.arg0 = 42323; m.arg1 = 33069; return m; }(), {5992.0, 235.0, 30.0, 42323.0, 33069.0}},
    {{0x98, 0xE5, 0xD9, 0x0E, 0x5D, 0x0E, 0xF0, 0x44}, [] { proto::TestControl m{}; m.magic = 58776; m.target_node = static_cast<proto::Node>(217); m.command = static_cast<proto::TestCommand>(14); m.arg0 = 3677; m.arg1 = 17648; return m; }(), {58776.0, 217.0, 14.0, 3677.0, 17648.0}},
    {{0x29, 0xA1, 0x25, 0xCF, 0x4A, 0xBA, 0x35, 0x09}, [] { proto::TestControl m{}; m.magic = 41257; m.target_node = static_cast<proto::Node>(37); m.command = static_cast<proto::TestCommand>(207); m.arg0 = 47690; m.arg1 = 2357; return m; }(), {41257.0, 37.0, 207.0, 47690.0, 2357.0}},
    {{0xA3, 0xD3, 0x4F, 0x75, 0xA9, 0x05, 0x62, 0x8D}, [] { proto::TestControl m{}; m.magic = 54179; m.target_node = static_cast<proto::Node>(79); m.command = static_cast<proto::TestCommand>(117); m.arg0 = 1449; m.arg1 = 36194; return m; }(), {54179.0, 79.0, 117.0, 1449.0, 36194.0}},
    {{0xA1, 0xB2, 0x7D, 0x06, 0xE2, 0xE3, 0xC4, 0x8D}, [] { proto::TestControl m{}; m.magic = 45729; m.target_node = static_cast<proto::Node>(125); m.command = static_cast<proto::TestCommand>(6); m.arg0 = 58338; m.arg1 = 36292; return m; }(), {45729.0, 125.0, 6.0, 58338.0, 36292.0}},
    {{0xF5, 0x15, 0x26, 0xE2, 0x85, 0x60, 0x1B, 0xE2}, [] { proto::TestControl m{}; m.magic = 5621; m.target_node = static_cast<proto::Node>(38); m.command = static_cast<proto::TestCommand>(226); m.arg0 = 24709; m.arg1 = 57883; return m; }(), {5621.0, 38.0, 226.0, 24709.0, 57883.0}},
    {{0x1F, 0xA0, 0xAF, 0x9D, 0x82, 0x2E, 0x4D, 0x0D}, [] { proto::TestControl m{}; m.magic = 40991; m.target_node = static_cast<proto::Node>(175); m.command = static_cast<proto::TestCommand>(157); m.arg0 = 11906; m.arg1 = 3405; return m; }(), {40991.0, 175.0, 157.0, 11906.0, 3405.0}},
    {{0x60, 0xD9, 0xD0, 0xF9, 0x03, 0x70, 0x66, 0x00}, [] { proto::TestControl m{}; m.magic = 55648; m.target_node = static_cast<proto::Node>(208); m.command = static_cast<proto::TestCommand>(249); m.arg0 = 28675; m.arg1 = 102; return m; }(), {55648.0, 208.0, 249.0, 28675.0, 102.0}},
    {{0xED, 0xFA, 0x34, 0xBC, 0x20, 0xED, 0x7C, 0x3D}, [] { proto::TestControl m{}; m.magic = 64237; m.target_node = static_cast<proto::Node>(52); m.command = static_cast<proto::TestCommand>(188); m.arg0 = 60704; m.arg1 = 15740; return m; }(), {64237.0, 52.0, 188.0, 60704.0, 15740.0}},
    {{0x26, 0x66, 0xA2, 0x45, 0xF9, 0x41, 0x2D, 0x6A}, [] { proto::TestControl m{}; m.magic = 26150; m.target_node = static_cast<proto::Node>(162); m.command = static_cast<proto::TestCommand>(69); m.arg0 = 16889; m.arg1 = 27181; return m; }(), {26150.0, 162.0, 69.0, 16889.0, 27181.0}},
    {{0x37, 0xAE, 0xDD, 0xB7, 0x65, 0x01, 0x4A, 0x4E}, [] { proto::TestControl m{}; m.magic = 44599; m.target_node = static_cast<proto::Node>(221); m.command = static_cast<proto::TestCommand>(183); m.arg0 = 357; m.arg1 = 20042; return m; }(), {44599.0, 221.0, 183.0, 357.0, 20042.0}},
    {{0x5B, 0x64, 0x74, 0x3A, 0xB3, 0x61, 0x8F, 0xD8}, [] { proto::TestControl m{}; m.magic = 25691; m.target_node = static_cast<proto::Node>(116); m.command = static_cast<proto::TestCommand>(58); m.arg0 = 25011; m.arg1 = 55439; return m; }(), {25691.0, 116.0, 58.0, 25011.0, 55439.0}},
}};

/// Random (identifier, frame) pairs with the CRC computed by can/e2e_ref.py.
struct CrcVector {
    std::uint32_t id{};
    std::array<std::uint8_t, 8> bytes{};
    std::uint8_t crc{};
};

inline constexpr std::array<CrcVector, 64> kCrc = {{
    {0x63EU, {0x2F, 0x99, 0x83, 0x3C, 0xF9, 0x22, 0x56, 0xF4}, 0x76},
    {0x123U, {0xD7, 0x76, 0x1E, 0x2D, 0x10, 0x32, 0x49, 0x29}, 0x7C},
    {0x333U, {0x78, 0x27, 0x2F, 0x64, 0x3E, 0xC6, 0x35, 0xD3}, 0x9F},
    {0x2E3U, {0x2A, 0xA9, 0xC3, 0x47, 0x2E, 0xFA, 0x77, 0x7A}, 0xC2},
    {0x262U, {0xC0, 0x8C, 0x0F, 0x1C, 0xC8, 0xD0, 0xF8, 0x0B}, 0x03},
    {0x681U, {0x1F, 0xAC, 0x49, 0x1B, 0x2E, 0x80, 0x30, 0xD9}, 0x91},
    {0x302U, {0xBE, 0x77, 0xE6, 0xFF, 0xEB, 0xF9, 0x73, 0x72}, 0xE5},
    {0x1ABU, {0x98, 0xAB, 0x0C, 0x89, 0x75, 0xEA, 0x2F, 0xC8}, 0x66},
    {0x745U, {0xD1, 0x67, 0x23, 0xDA, 0x31, 0xDB, 0xC7, 0xE9}, 0xF3},
    {0x4F4U, {0xBC, 0x38, 0x19, 0x5B, 0x71, 0x13, 0xC9, 0x08}, 0x5E},
    {0x6E1U, {0x2A, 0xBD, 0x91, 0xCD, 0x8C, 0x73, 0x70, 0x6E}, 0x3F},
    {0x563U, {0xC6, 0x25, 0x93, 0x83, 0x4F, 0xC3, 0x8B, 0x52}, 0x0C},
    {0x73FU, {0x3B, 0x1B, 0xB4, 0x90, 0x13, 0x98, 0xFE, 0xF4}, 0x73},
    {0x28CU, {0x73, 0xAC, 0x1F, 0xD4, 0xD9, 0xCA, 0x79, 0x64}, 0x39},
    {0x6A5U, {0x78, 0xB3, 0x35, 0xB5, 0x5B, 0xB4, 0xED, 0xAD}, 0xB8},
    {0x26DU, {0xD2, 0x53, 0x85, 0x6A, 0x54, 0x52, 0x54, 0x8A}, 0xAB},
    {0x373U, {0x29, 0x7B, 0x0F, 0x90, 0x9B, 0xC0, 0xE7, 0x06}, 0x1B},
    {0x0ADU, {0xAF, 0x45, 0xDA, 0xA9, 0x85, 0x63, 0x65, 0x9F}, 0x2B},
    {0x4EAU, {0x04, 0x3C, 0x07, 0xF1, 0x3A, 0x04, 0x99, 0xC4}, 0x11},
    {0x18CU, {0x03, 0x50, 0x1C, 0xC6, 0xAB, 0x85, 0x4A, 0x4E}, 0x36},
    {0x72AU, {0x36, 0x1D, 0x01, 0x96, 0xDE, 0x28, 0xEF, 0x53}, 0x99},
    {0x263U, {0x7E, 0x06, 0x21, 0xD8, 0xFD, 0xFD, 0x08, 0xE7}, 0xE7},
    {0x433U, {0xF3, 0x77, 0x40, 0xDD, 0x23, 0xB8, 0x44, 0xF8}, 0xED},
    {0x5DAU, {0x92, 0xF2, 0xB3, 0x74, 0xC6, 0x4B, 0x7B, 0xAA}, 0x51},
    {0x777U, {0xBF, 0x06, 0x56, 0x8F, 0xBE, 0xCB, 0xDA, 0x6B}, 0x02},
    {0x60AU, {0x2D, 0xB5, 0x03, 0xC0, 0x1B, 0x47, 0xDB, 0x30}, 0xF7},
    {0x69BU, {0x5A, 0x07, 0xA5, 0x36, 0x82, 0x31, 0x48, 0x42}, 0x76},
    {0x328U, {0x6D, 0x47, 0x6D, 0xAA, 0x48, 0xEC, 0xA0, 0xA0}, 0xF9},
    {0x4FDU, {0x3A, 0x6A, 0x15, 0xFC, 0x96, 0x64, 0x62, 0x50}, 0xDC},
    {0x345U, {0x8F, 0x7E, 0x6B, 0x63, 0x13, 0xF2, 0x0E, 0x0B}, 0x00},
    {0x71AU, {0x81, 0xFA, 0x91, 0xEA, 0xAC, 0x3F, 0x2D, 0x31}, 0xDC},
    {0x72FU, {0xEB, 0x1A, 0x89, 0xC1, 0x43, 0xB6, 0xBF, 0x40}, 0x0E},
    {0x6D2U, {0x5F, 0x22, 0x1D, 0x64, 0x23, 0xD6, 0x76, 0x5E}, 0xDA},
    {0x61EU, {0x06, 0x5F, 0x20, 0x9B, 0x72, 0x02, 0x5F, 0xE0}, 0x41},
    {0x0E4U, {0xB6, 0xF4, 0x05, 0x3B, 0xD4, 0x1A, 0xEB, 0x91}, 0x8B},
    {0x474U, {0x5E, 0x7F, 0x25, 0xB6, 0x39, 0xAB, 0x54, 0x0B}, 0x07},
    {0x0FAU, {0xFD, 0x3E, 0x65, 0xDD, 0xA0, 0x24, 0xA5, 0x11}, 0x61},
    {0x3C9U, {0x03, 0xB1, 0x21, 0xEA, 0x43, 0x03, 0x38, 0xCD}, 0xB2},
    {0x454U, {0x20, 0xC5, 0xF6, 0xE8, 0xF5, 0xD8, 0xE2, 0x14}, 0x6A},
    {0x0D1U, {0xE7, 0x58, 0xEF, 0xAF, 0xE5, 0x84, 0x10, 0x34}, 0x45},
    {0x7BAU, {0x62, 0x92, 0x00, 0xAC, 0x2C, 0x44, 0x45, 0x14}, 0x4A},
    {0x0BDU, {0x42, 0xBA, 0xBA, 0x89, 0xA5, 0x06, 0xDA, 0xEF}, 0xE6},
    {0x791U, {0x94, 0x52, 0x36, 0x28, 0x38, 0x92, 0xCB, 0xF5}, 0x4D},
    {0x52EU, {0x58, 0x3C, 0x82, 0x1A, 0x13, 0xAA, 0x67, 0x4E}, 0xAD},
    {0x28FU, {0xA9, 0xED, 0xC6, 0x8E, 0x06, 0x8F, 0x9A, 0xC3}, 0x52},
    {0x082U, {0xB7, 0x27, 0xB2, 0x7D, 0xBF, 0x17, 0x38, 0x94}, 0x0D},
    {0x7E3U, {0x2E, 0xEA, 0x6D, 0xB3, 0x7D, 0xE8, 0x9A, 0x61}, 0x4B},
    {0x07EU, {0x74, 0x4B, 0x06, 0x95, 0x90, 0xF9, 0x2E, 0x42}, 0x59},
    {0x7CAU, {0x70, 0x0A, 0xE7, 0x49, 0x95, 0x37, 0x22, 0xB2}, 0x2C},
    {0x47DU, {0x2D, 0x41, 0xF8, 0x6B, 0xDF, 0x79, 0xDD, 0x20}, 0x88},
    {0x17FU, {0x72, 0x3D, 0xC2, 0xF1, 0x32, 0x54, 0xDF, 0xAA}, 0x48},
    {0x6D0U, {0x94, 0xDC, 0x08, 0x9C, 0xF5, 0xF2, 0x19, 0xF1}, 0xC3},
    {0x571U, {0x72, 0x5C, 0xDA, 0xF4, 0x9D, 0xC4, 0x8D, 0xD8}, 0xBE},
    {0x71EU, {0x36, 0x67, 0x08, 0x3A, 0xA8, 0xB8, 0xA1, 0xF8}, 0x11},
    {0x479U, {0x10, 0x70, 0x56, 0x9D, 0x43, 0xE8, 0x0F, 0x86}, 0xA0},
    {0x425U, {0xBC, 0x35, 0x34, 0x85, 0x29, 0x7E, 0xC3, 0x28}, 0xC2},
    {0x162U, {0x09, 0x02, 0xAD, 0x98, 0xE1, 0xD7, 0xDF, 0x1F}, 0x0B},
    {0x43BU, {0xA4, 0xAD, 0x7C, 0xE9, 0xEE, 0xF3, 0x19, 0x84}, 0xEB},
    {0x67CU, {0xA3, 0x71, 0x4E, 0xC8, 0x87, 0xCB, 0x04, 0x1E}, 0xB1},
    {0x5B4U, {0x0F, 0xCE, 0xAA, 0x5F, 0x55, 0x68, 0x3B, 0x1E}, 0x2C},
    {0x687U, {0xF7, 0x8F, 0x4A, 0xA1, 0x4A, 0x19, 0x81, 0xC7}, 0x0A},
    {0x0A9U, {0x03, 0xE8, 0x28, 0x6D, 0xF4, 0x29, 0xA7, 0xB3}, 0xD6},
    {0x6D5U, {0x6B, 0xE7, 0xAA, 0xB0, 0xE3, 0x89, 0xC9, 0x6B}, 0x07},
    {0x175U, {0x8F, 0x43, 0x2A, 0x9A, 0x01, 0x9F, 0x19, 0x74}, 0x88},
}};

/// Plain frame (crc byte 0) + counter -> protected frame, per can/e2e_ref.py.
struct E2eVector {
    std::uint32_t id{};
    std::array<std::uint8_t, 8> plain{};
    std::uint8_t counter{};
    std::array<std::uint8_t, 8> protected_frame{};
};

inline constexpr std::array<E2eVector, 20> kE2e = {{
    {0x080U, {0x00, 0xC0, 0x00, 0xE8, 0x74, 0x54, 0x06, 0x96}, 0, {0x59, 0xC0, 0x00, 0xE8, 0x74, 0x54, 0x06, 0x96}},
    {0x080U, {0x00, 0x80, 0x56, 0x74, 0xED, 0x1E, 0x02, 0xB1}, 1, {0xA0, 0x81, 0x56, 0x74, 0xED, 0x1E, 0x02, 0xB1}},
    {0x080U, {0x00, 0xA0, 0x04, 0x8F, 0x44, 0x0F, 0x01, 0xCE}, 7, {0xBB, 0xA7, 0x04, 0x8F, 0x44, 0x0F, 0x01, 0xCE}},
    {0x080U, {0x00, 0x10, 0x0A, 0x01, 0xEA, 0xD8, 0x02, 0xE9}, 15, {0x47, 0x1F, 0x0A, 0x01, 0xEA, 0xD8, 0x02, 0xE9}},
    {0x100U, {0x00, 0x00, 0x2E, 0x0B, 0x00, 0x35, 0x29, 0x99}, 0, {0x61, 0x00, 0x2E, 0x0B, 0x00, 0x35, 0x29, 0x99}},
    {0x100U, {0x00, 0x90, 0x9E, 0x08, 0x04, 0xF1, 0x61, 0x29}, 1, {0x24, 0x91, 0x9E, 0x08, 0x04, 0xF1, 0x61, 0x29}},
    {0x100U, {0x00, 0x20, 0x22, 0x79, 0xC6, 0xAA, 0x43, 0xEA}, 7, {0xAF, 0x27, 0x22, 0x79, 0xC6, 0xAA, 0x43, 0xEA}},
    {0x100U, {0x00, 0xD0, 0x1C, 0x8F, 0xD0, 0x5D, 0x12, 0x78}, 15, {0xD4, 0xDF, 0x1C, 0x8F, 0xD0, 0x5D, 0x12, 0x78}},
    {0x110U, {0x00, 0x30, 0xF6, 0xF6, 0xCF, 0x3C, 0x2B, 0x7D}, 0, {0x11, 0x30, 0xF6, 0xF6, 0xCF, 0x3C, 0x2B, 0x7D}},
    {0x110U, {0x00, 0x20, 0xE0, 0x67, 0xC8, 0x48, 0x30, 0xB2}, 1, {0xF2, 0x21, 0xE0, 0x67, 0xC8, 0x48, 0x30, 0xB2}},
    {0x110U, {0x00, 0x00, 0x4E, 0x7C, 0x85, 0xDD, 0x42, 0x03}, 7, {0xD0, 0x07, 0x4E, 0x7C, 0x85, 0xDD, 0x42, 0x03}},
    {0x110U, {0x00, 0x30, 0xE4, 0xD0, 0x0C, 0xE1, 0x6C, 0x3C}, 15, {0x57, 0x3F, 0xE4, 0xD0, 0x0C, 0xE1, 0x6C, 0x3C}},
    {0x200U, {0x00, 0x00, 0x82, 0x31, 0x52, 0xEF, 0x4B, 0x35}, 0, {0x8E, 0x00, 0x82, 0x31, 0x52, 0xEF, 0x4B, 0x35}},
    {0x200U, {0x00, 0x60, 0x00, 0xA3, 0xE6, 0x78, 0xFA, 0xDC}, 1, {0xF5, 0x61, 0x00, 0xA3, 0xE6, 0x78, 0xFA, 0xDC}},
    {0x200U, {0x00, 0xC0, 0xD8, 0xC0, 0x3A, 0xDF, 0xA8, 0xB8}, 7, {0x9C, 0xC7, 0xD8, 0xC0, 0x3A, 0xDF, 0xA8, 0xB8}},
    {0x200U, {0x00, 0x00, 0xAC, 0x62, 0xBD, 0x7B, 0xAE, 0x79}, 15, {0xA3, 0x0F, 0xAC, 0x62, 0xBD, 0x7B, 0xAE, 0x79}},
    {0x201U, {0x00, 0xB0, 0x72, 0x28, 0xA6, 0x60, 0xE7, 0x2C}, 0, {0x71, 0xB0, 0x72, 0x28, 0xA6, 0x60, 0xE7, 0x2C}},
    {0x201U, {0x00, 0x70, 0x29, 0x7E, 0x49, 0xC8, 0x2A, 0xE6}, 1, {0xFE, 0x71, 0x29, 0x7E, 0x49, 0xC8, 0x2A, 0xE6}},
    {0x201U, {0x00, 0x20, 0x49, 0x31, 0xF6, 0x9F, 0x64, 0x0B}, 7, {0x99, 0x27, 0x49, 0x31, 0xF6, 0x9F, 0x64, 0x0B}},
    {0x201U, {0x00, 0x40, 0x9E, 0x30, 0xBC, 0xB1, 0xCE, 0x55}, 15, {0x36, 0x4F, 0x9E, 0x30, 0xBC, 0xB1, 0xCE, 0x55}},
}};

/// Every value of every generated enum with its DBC label, and the width of the
/// signal carrying it (for to_string tests over the whole raw range).
template <class E>
struct EnumVector {
    E value{};
    const char* label{};
};

struct ControllerModeEnum {
    using Type = proto::ControllerMode;
    static constexpr unsigned kBits = 4;
    static constexpr std::array<EnumVector<proto::ControllerMode>, 4> kNames = {{
        {proto::ControllerMode::kInit, "INIT"},
        {proto::ControllerMode::kNormal, "NORMAL"},
        {proto::ControllerMode::kDegraded, "DEGRADED"},
        {proto::ControllerMode::kSafeState, "SAFE_STATE"},
    }};
};

struct SensorStatusEnum {
    using Type = proto::SensorStatus;
    static constexpr unsigned kBits = 4;
    static constexpr std::array<EnumVector<proto::SensorStatus>, 3> kNames = {{
        {proto::SensorStatus::kOk, "OK"},
        {proto::SensorStatus::kDegraded, "DEGRADED"},
        {proto::SensorStatus::kFault, "FAULT"},
    }};
};

struct NodeStateEnum {
    using Type = proto::NodeState;
    static constexpr unsigned kBits = 4;
    static constexpr std::array<EnumVector<proto::NodeState>, 5> kNames = {{
        {proto::NodeState::kBoot, "BOOT"},
        {proto::NodeState::kRunning, "RUNNING"},
        {proto::NodeState::kDegraded, "DEGRADED"},
        {proto::NodeState::kSafeState, "SAFE_STATE"},
        {proto::NodeState::kFault, "FAULT"},
    }};
};

struct CanStateEnum {
    using Type = proto::CanState;
    static constexpr unsigned kBits = 2;
    static constexpr std::array<EnumVector<proto::CanState>, 4> kNames = {{
        {proto::CanState::kErrorActive, "ERROR_ACTIVE"},
        {proto::CanState::kErrorPassive, "ERROR_PASSIVE"},
        {proto::CanState::kBusOff, "BUS_OFF"},
        {proto::CanState::kStopped, "STOPPED"},
    }};
};

struct NodeEnum {
    using Type = proto::Node;
    static constexpr unsigned kBits = 8;
    static constexpr std::array<EnumVector<proto::Node>, 3> kNames = {{
        {proto::Node::kAll, "ALL"},
        {proto::Node::kSensorEcu, "SENSOR_ECU"},
        {proto::Node::kControllerEcu, "CONTROLLER_ECU"},
    }};
};

struct TestCommandEnum {
    using Type = proto::TestCommand;
    static constexpr unsigned kBits = 8;
    static constexpr std::array<EnumVector<proto::TestCommand>, 10> kNames = {{
        {proto::TestCommand::kNop, "NOP"},
        {proto::TestCommand::kResetFaults, "RESET_FAULTS"},
        {proto::TestCommand::kInjectCanError, "INJECT_CAN_ERROR"},
        {proto::TestCommand::kForceBusOff, "FORCE_BUS_OFF"},
        {proto::TestCommand::kSuppressHeartbeat, "SUPPRESS_HEARTBEAT"},
        {proto::TestCommand::kCorruptCrc, "CORRUPT_CRC"},
        {proto::TestCommand::kSkipCounter, "SKIP_COUNTER"},
        {proto::TestCommand::kFreezeCounter, "FREEZE_COUNTER"},
        {proto::TestCommand::kSuppressMessage, "SUPPRESS_MESSAGE"},
        {proto::TestCommand::kStallThread, "STALL_THREAD"},
    }};
};

}  // namespace vectors

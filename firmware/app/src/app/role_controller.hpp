// Compile-time constants of the controller ECU role (selected by CONFIG_APP_ROLE_CONTROLLER).
#pragma once

#include <array>
#include <cstddef>

#include "app/config.hpp"
#include "ecu/node_state.hpp"
#include "ecu/timeout_monitor.hpp"
#include "protocol/messages.hpp"

namespace app::role {

constexpr const char* kName = "controller";
constexpr proto::Node kNode = proto::Node::kControllerEcu;
using Heartbeat = proto::ControllerHeartbeat;
using EchoResponse = proto::ControllerEchoResponse;

// Sensor data and the sensor heartbeat are critical: losing either puts the
// controller in its safe state. Battery status only derates (DEGRADED).
constexpr std::size_t kMonitoredCount = 3;
constexpr std::array<ecu::MonitoredMessage, kMonitoredCount> kMonitored{{
    {proto::SensorData::kId, cfg::kSensorDataTimeoutMs, proto::SensorData::kName},
    {proto::SensorHeartbeat::kId, cfg::kHeartbeatTimeoutMs, proto::SensorHeartbeat::kName},
    {proto::BatteryStatus::kId, cfg::kBatteryStatusTimeoutMs, proto::BatteryStatus::kName},
}};
constexpr ecu::NodePolicy kNodePolicy{true, cfg::kSafeStateRecoveryMs};

}  // namespace app::role

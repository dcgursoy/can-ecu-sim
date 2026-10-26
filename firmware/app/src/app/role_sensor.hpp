// Compile-time constants of the sensor ECU role (selected by CONFIG_APP_ROLE_SENSOR).
#pragma once

#include <array>
#include <cstddef>

#include "app/config.hpp"
#include "ecu/node_state.hpp"
#include "ecu/timeout_monitor.hpp"
#include "protocol/messages.hpp"

namespace app::role {

constexpr const char* kName = "sensor";
constexpr proto::Node kNode = proto::Node::kSensorEcu;
using Heartbeat = proto::SensorHeartbeat;
using EchoResponse = proto::SensorEchoResponse;

// The sensor watches the controller to report its own view of the network; it
// has no actuator, so losing the controller makes it DEGRADED, not safe.
constexpr std::size_t kMonitoredCount = 2;
constexpr std::array<ecu::MonitoredMessage, kMonitoredCount> kMonitored{{
    {proto::ControllerCommand::kId, cfg::kCommandTimeoutMs, proto::ControllerCommand::kName},
    {proto::ControllerHeartbeat::kId, cfg::kHeartbeatTimeoutMs, proto::ControllerHeartbeat::kName},
}};
constexpr ecu::NodePolicy kNodePolicy{false, cfg::kSafeStateRecoveryMs};

}  // namespace app::role

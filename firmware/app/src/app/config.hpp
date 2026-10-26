// Compile-time configuration of the application, derived from Kconfig so every
// number has one home (firmware/app/Kconfig) and one explanation (DESIGN.md).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ecu/can_error_sm.hpp"
#include "ecu/node_state.hpp"
#include "protocol/messages.hpp"

namespace app::cfg {

// TX scheduler base period: the greatest common divisor of all cycle times.
constexpr std::uint32_t kBaseTickMs = 10;
static_assert(proto::SensorData::kCycleMs % kBaseTickMs == 0, "cycle time must be a multiple of the base tick");
static_assert(proto::ControllerCommand::kCycleMs % kBaseTickMs == 0, "cycle time must be a multiple of the base tick");
static_assert(proto::BatteryStatus::kCycleMs % kBaseTickMs == 0, "cycle time must be a multiple of the base tick");
static_assert(proto::SensorHeartbeat::kCycleMs % kBaseTickMs == 0, "cycle time must be a multiple of the base tick");
static_assert(proto::ControllerHeartbeat::kCycleMs % kBaseTickMs == 0, "cycle time must be a multiple of the base tick");

constexpr std::uint32_t kSupervisorTickMs = CONFIG_APP_SUPERVISOR_TICK_MS;
constexpr std::uint32_t kRxQueueDepth = CONFIG_APP_RX_QUEUE_DEPTH;

constexpr std::uint32_t kSensorDataTimeoutMs = CONFIG_APP_SENSOR_DATA_TIMEOUT_MS;
constexpr std::uint32_t kBatteryStatusTimeoutMs = CONFIG_APP_BATTERY_STATUS_TIMEOUT_MS;
constexpr std::uint32_t kCommandTimeoutMs = CONFIG_APP_COMMAND_TIMEOUT_MS;
constexpr std::uint32_t kHeartbeatTimeoutMs = CONFIG_APP_HEARTBEAT_TIMEOUT_MS;
constexpr std::uint32_t kSafeStateRecoveryMs = CONFIG_APP_SAFE_STATE_RECOVERY_MS;
constexpr std::uint32_t kQualityFaultThreshold = CONFIG_APP_QUALITY_FAULT_THRESHOLD;

static_assert(kSensorDataTimeoutMs > proto::SensorData::kCycleMs, "timeout must exceed the period");
static_assert(kCommandTimeoutMs > proto::ControllerCommand::kCycleMs, "timeout must exceed the period");
static_assert(kHeartbeatTimeoutMs > proto::SensorHeartbeat::kCycleMs, "timeout must exceed the period");

constexpr ecu::BusOffPolicy kBusOffPolicy{
    CONFIG_APP_BUSOFF_BACKOFF_INITIAL_MS,
    CONFIG_APP_BUSOFF_BACKOFF_MAX_MS,
    CONFIG_APP_BUSOFF_RECOVERY_TIMEOUT_MS,
    CONFIG_APP_BUSOFF_MAX_ATTEMPTS,
    CONFIG_APP_BUSOFF_STABLE_MS,
};

constexpr std::uint32_t kWatchdogTimeoutMs = CONFIG_APP_WATCHDOG_TIMEOUT_MS;
constexpr std::uint32_t kLivenessWindowMs = CONFIG_APP_LIVENESS_WINDOW_MS;
static_assert(kLivenessWindowMs < kWatchdogTimeoutMs, "a stale thread must be noticed before the watchdog bites");

// Thread priorities (preemptive; lower number runs first). On native_sim the CAN
// driver's own receive thread runs at 2 and must stay ahead of everything else,
// so frames reach the queue before the application touches them.
constexpr int kRxThreadPriority = 3;          // bounded per-frame work, must not let the queue fill
constexpr int kTxThreadPriority = 4;          // periodic frames; jitter here is jitter on the bus
constexpr int kSupervisorThreadPriority = 5;  // 5 ms control plane: timeouts, recovery, watchdog
constexpr std::size_t kThreadStackSize = CONFIG_APP_THREAD_STACK_SIZE;

}  // namespace app::cfg

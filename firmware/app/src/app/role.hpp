// The role interface: what differs between the sensor and the controller ECU.
// The role header selected by Kconfig supplies the compile-time constants
// (name, node id, monitored messages, node policy, message types); the matching
// source (sensor_role.cpp or controller_role.cpp) implements the functions.
// Everything else (threads, supervisor,
// CAN error handling, watchdog, echo, test hooks, shell) is common code.
#pragma once

#include <zephyr/drivers/can.h>

#include <cstdint>

#if defined(CONFIG_APP_ROLE_SENSOR)
#include "app/role_sensor.hpp"
#elif defined(CONFIG_APP_ROLE_CONTROLLER)
#include "app/role_controller.hpp"
#else
#error "select an ECU role: CONFIG_APP_ROLE_SENSOR or CONFIG_APP_ROLE_CONTROLLER"
#endif

#include "app/status.h"
#include "ecu/node_state.hpp"

namespace app::role {

/// One-time initialisation before the threads start.
void init(std::int64_t now_ms);

/// RX thread: a standard data frame that the common layer did not consume.
/// Returns false if the identifier is not one this role listens to.
bool handle_frame(const can_frame& frame, std::int64_t now_ms);

/// TX thread: called every cfg::kBaseTickMs with a running tick count.
void periodic_tx(std::uint32_t tick, std::int64_t now_ms);

/// Supervisor: the node state machine's view of this role's inputs.
ecu::NodeInputs inputs(std::int64_t now_ms);

/// Supervisor: a monitored message timed out (forget its counter history).
void on_message_lost(std::uint32_t id);

/// Supervisor: the node state changed (e.g. force safe outputs).
void on_node_state(proto::NodeState state);

/// Clear role statistics (shell `ecu reset`, TEST_CONTROL RESET_FAULTS).
void reset_stats();

/// Shell: per-message receive statistics and a one-line role detail.
void fill_status(ecu_status& out, std::int64_t now_ms);

}  // namespace app::role

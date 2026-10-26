// Controller ECU: consumes sensor data, battery status and the sensor heartbeat,
// publishes an actuator command every 20 ms and its heartbeat every 100 ms.
// Loss of sensor data or of the sensor heartbeat puts it in its safe state.
//
// Safe state (also published in CONTROLLER_COMMAND.mode so it is visible on the
// bus): actuators disabled, torque and steering commands zero, brake held,
// speed limit zero. Commands keep being transmitted at their normal rate so an
// actuator node sees the safe command rather than silence.
#include <zephyr/logging/log.h>

#include <cstdio>

#include "app/ecu.hpp"
#include "app/role.hpp"
#include "ecu/protected_rx.hpp"

LOG_MODULE_REGISTER(role_controller, LOG_LEVEL_INF);

namespace app::role {

namespace {

ecu::ProtectedReceiver<proto::SensorData> g_sensor_rx;
ecu::ProtectedReceiver<proto::BatteryStatus> g_battery_rx;
ecu::ProtectedReceiver<proto::SensorHeartbeat> g_heartbeat_rx;
proto::SensorData g_sensor{};
proto::BatteryStatus g_battery{};
bool g_have_sensor = false;
bool g_have_battery = false;
proto::e2e::CounterGenerator g_command_counter;
proto::e2e::CounterGenerator g_heartbeat_counter;
proto::ControllerCommand g_last_command{};
std::uint32_t g_tx_command = 0;
std::uint32_t g_tx_heartbeat = 0;

constexpr std::uint32_t kHeartbeatSlot = 6;  // 10 ms tick within the 100 ms cycle

// Plain proportional speed controller with yaw damping; the point is a
// deterministic, explainable command, not vehicle dynamics.
constexpr float kSpeedSetpointMps = 15.0F;
constexpr float kSpeedLimitNominalMps = 25.0F;
constexpr float kSpeedLimitDeratedMps = 10.0F;
constexpr float kTorqueGainNmPerMps = 20.0F;
constexpr float kTorqueLimitNm = 50.0F;
constexpr float kSteeringGainDegPerDps = -0.5F;
constexpr float kSteeringLimitDeg = 30.0F;

constexpr float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

proto::ControllerMode to_mode(proto::NodeState state)
{
    switch (state) {
    case proto::NodeState::kRunning: return proto::ControllerMode::kNormal;
    case proto::NodeState::kDegraded: return proto::ControllerMode::kDegraded;
    case proto::NodeState::kSafeState:
    case proto::NodeState::kFault: return proto::ControllerMode::kSafeState;
    case proto::NodeState::kBoot:
    default: return proto::ControllerMode::kInit;
    }
}

proto::ControllerCommand compute_command(proto::NodeState state, const proto::SensorData& sensor, bool have_sensor,
                                         const proto::BatteryStatus& battery, bool have_battery)
{
    proto::ControllerCommand cmd{};
    cmd.mode = to_mode(state);
    const bool safe = state != proto::NodeState::kRunning && state != proto::NodeState::kDegraded;
    if (safe || !have_sensor) {
        cmd.torque_cmd = 0;
        cmd.steering_cmd = 0;
        cmd.actuators_enabled = 0;
        cmd.brake_hold = 1;
        cmd.fault_latched = state == proto::NodeState::kFault ? 1 : 0;
        cmd.speed_limit = 0;
        return cmd;
    }
    const bool derate = state == proto::NodeState::kDegraded ||
                        (have_battery && (battery.low_voltage != 0U || battery.over_temperature != 0U));
    const float speed_limit = derate ? kSpeedLimitDeratedMps : kSpeedLimitNominalMps;
    const float setpoint = clamp(kSpeedSetpointMps, 0.0F, speed_limit);
    cmd.set_torque_cmd_phys(clamp(kTorqueGainNmPerMps * (setpoint - sensor.speed_phys()), -kTorqueLimitNm, kTorqueLimitNm));
    cmd.set_steering_cmd_phys(clamp(kSteeringGainDegPerDps * sensor.yaw_rate_phys(), -kSteeringLimitDeg, kSteeringLimitDeg));
    cmd.set_speed_limit_phys(speed_limit);
    cmd.actuators_enabled = 1;
    cmd.brake_hold = 0;
    cmd.fault_latched = 0;
    return cmd;
}

}  // namespace

void init(std::int64_t /*now_ms*/) {}

bool handle_frame(const can_frame& frame, std::int64_t now_ms)
{
    ecu::RxResult result{};
    switch (frame.id) {
    case proto::SensorData::kId: {
        proto::SensorData data{};
        {
            Locked l(shared().lock);
            result = g_sensor_rx.receive(frame.data, frame.dlc, data);
            if (ecu::accepted(result)) {
                shared().monitor.note_received(frame.id, now_ms);
                g_sensor = data;
                g_have_sensor = true;
            }
        }
        note_rx_result(result);
        return true;
    }
    case proto::BatteryStatus::kId: {
        proto::BatteryStatus battery{};
        {
            Locked l(shared().lock);
            result = g_battery_rx.receive(frame.data, frame.dlc, battery);
            if (ecu::accepted(result)) {
                shared().monitor.note_received(frame.id, now_ms);
                g_battery = battery;
                g_have_battery = true;
            }
        }
        note_rx_result(result);
        return true;
    }
    case proto::SensorHeartbeat::kId: {
        proto::SensorHeartbeat hb{};
        {
            Locked l(shared().lock);
            result = g_heartbeat_rx.receive(frame.data, frame.dlc, hb);
            if (ecu::accepted(result)) {
                shared().monitor.note_received(frame.id, now_ms);
            }
        }
        note_rx_result(result);
        return true;
    }
    default:
        return false;
    }
}

void periodic_tx(std::uint32_t tick, std::int64_t now_ms)
{
    constexpr std::uint32_t kTicksPerCommand = proto::ControllerCommand::kCycleMs / cfg::kBaseTickMs;
    constexpr std::uint32_t kTicksPer100ms = proto::ControllerHeartbeat::kCycleMs / cfg::kBaseTickMs;

    if (tick % kTicksPerCommand == 0) {
        proto::NodeState state{};
        proto::SensorData sensor{};
        proto::BatteryStatus battery{};
        bool have_sensor = false;
        bool have_battery = false;
        {
            Locked l(shared().lock);
            state = shared().node.state();
            sensor = g_sensor;
            battery = g_battery;
            have_sensor = g_have_sensor && shared().monitor.alive(proto::SensorData::kId);
            have_battery = g_have_battery && shared().monitor.alive(proto::BatteryStatus::kId);
        }
        const proto::ControllerCommand cmd = compute_command(state, sensor, have_sensor, battery, have_battery);
        if (transmit<proto::ControllerCommand>(proto::e2e::protect(cmd, g_command_counter.next())) == 0) {
            ++g_tx_command;
        }
        Locked l(shared().lock);
        g_last_command = cmd;
    }
    if (tick % kTicksPer100ms == kHeartbeatSlot) {
        if (transmit<Heartbeat>(make_heartbeat<Heartbeat>(g_heartbeat_counter, now_ms)) == 0) {
            ++g_tx_heartbeat;
        }
    }
}

ecu::NodeInputs inputs(std::int64_t /*now_ms*/)
{
    Locked l(shared().lock);
    ecu::NodeInputs in;
    const auto& m = shared().monitor;
    in.critical_inputs_alive = m.alive(proto::SensorData::kId) && m.alive(proto::SensorHeartbeat::kId);
    in.critical_inputs_lost = m.lost(proto::SensorData::kId) || m.lost(proto::SensorHeartbeat::kId);
    const bool quality_faults = g_sensor_rx.stats().consecutive_rejects >= cfg::kQualityFaultThreshold ||
                                g_battery_rx.stats().consecutive_rejects >= cfg::kQualityFaultThreshold ||
                                g_heartbeat_rx.stats().consecutive_rejects >= cfg::kQualityFaultThreshold;
    const bool battery_warning = g_have_battery && (g_battery.low_voltage != 0U || g_battery.over_temperature != 0U);
    in.degraded_quality = quality_faults || !m.alive(proto::BatteryStatus::kId) || battery_warning ||
                          (g_have_sensor && g_sensor.sensor_status != proto::SensorStatus::kOk);
    in.can_failed = shared().can_sm.failed();
    in.bus_off = shared().can_sm.bus_off();
    return in;
}

void on_message_lost(std::uint32_t id)
{
    Locked l(shared().lock);
    if (id == proto::SensorData::kId) {
        g_sensor_rx.reset_sequence();
    } else if (id == proto::BatteryStatus::kId) {
        g_battery_rx.reset_sequence();
    } else if (id == proto::SensorHeartbeat::kId) {
        g_heartbeat_rx.reset_sequence();
    }
}

void on_node_state(proto::NodeState state)
{
    if (state == proto::NodeState::kSafeState || state == proto::NodeState::kFault) {
        LOG_WRN("safe state: actuators disabled, brake hold, speed limit 0");
    }
}

void reset_stats()
{
    Locked l(shared().lock);
    g_sensor_rx.reset_stats();
    g_battery_rx.reset_stats();
    g_heartbeat_rx.reset_stats();
}

namespace {

void fill_rx(ecu_rx_status& out, const char* name, const ecu::RxStats& s, std::uint32_t id, std::int64_t now_ms)
{
    out.name = name;
    out.accepted = s.accepted;
    out.crc_faults = s.crc_faults;
    out.counter_faults = s.repeated + s.out_of_sequence;
    out.dlc_faults = s.dlc_faults;
    out.skipped = s.skipped;
    out.lost_frames = s.lost_frames;
    const auto* entry = shared().monitor.find(id);
    out.timeouts = entry != nullptr ? entry->timeouts : 0U;
    out.age_ms = shared().monitor.age_ms(id, now_ms);
    out.alive = shared().monitor.alive(id) ? 1 : 0;
}

}  // namespace

void fill_status(ecu_status& out, std::int64_t now_ms)
{
    // Snapshot under the lock, format outside it: the spinlock masks interrupts.
    std::uint32_t tx_command = 0;
    std::uint32_t tx_heartbeat = 0;
    proto::ControllerCommand cmd{};
    std::uint16_t sensor_speed = 0;
    std::uint8_t soc = 0;
    {
        Locked l(shared().lock);
        out.rx_count = 3;
        fill_rx(out.rx[0], proto::SensorData::kName, g_sensor_rx.stats(), proto::SensorData::kId, now_ms);
        fill_rx(out.rx[1], proto::SensorHeartbeat::kName, g_heartbeat_rx.stats(), proto::SensorHeartbeat::kId,
                now_ms);
        fill_rx(out.rx[2], proto::BatteryStatus::kName, g_battery_rx.stats(), proto::BatteryStatus::kId, now_ms);
        tx_command = g_tx_command;
        tx_heartbeat = g_tx_heartbeat;
        cmd = g_last_command;
        sensor_speed = g_sensor.speed;
        soc = g_battery.state_of_charge;
    }
    std::snprintf(out.role_detail, sizeof(out.role_detail),
                  "tx command=%u heartbeat=%u cmd: mode=%s torque_0.01Nm=%d steering_0.01deg=%d "
                  "speed_limit_0.1m_s=%u enabled=%u brake=%u | sensor speed_cm_s=%u soc_0.5pct=%u",
                  static_cast<unsigned>(tx_command), static_cast<unsigned>(tx_heartbeat),
                  proto::to_string(cmd.mode), static_cast<int>(cmd.torque_cmd), static_cast<int>(cmd.steering_cmd),
                  static_cast<unsigned>(cmd.speed_limit), static_cast<unsigned>(cmd.actuators_enabled),
                  static_cast<unsigned>(cmd.brake_hold), static_cast<unsigned>(sensor_speed),
                  static_cast<unsigned>(soc));
}

}  // namespace app::role

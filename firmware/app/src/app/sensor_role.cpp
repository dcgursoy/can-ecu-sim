// Sensor ECU: publishes simulated vehicle sensor data (10 ms), battery status
// (100 ms) and its heartbeat (100 ms); monitors the controller's command and
// heartbeat and reports their quality. It has no actuator, so losing the
// controller only makes it DEGRADED.
#include <zephyr/logging/log.h>

#include <cmath>
#include <cstdio>

#include "app/ecu.hpp"
#include "app/role.hpp"
#include "ecu/protected_rx.hpp"

LOG_MODULE_REGISTER(role_sensor, LOG_LEVEL_INF);

namespace app::role {

namespace {

// Deterministic vehicle model: a slow speed swell, a yaw oscillation and a
// draining battery, all functions of uptime so two runs produce the same data.
struct SensorModel {
    float speed_mps = 0.0F;
    float yaw_rate_dps = 0.0F;
    float accel_x_mps2 = 0.0F;
    float soc_pct = 90.0F;
    float voltage_v = 48.0F;
    float current_a = 0.0F;
    float temperature_c = 25.0F;

    void step(std::int64_t now_ms)
    {
        constexpr float kTwoPi = 6.2831853F;
        const float t = static_cast<float>(now_ms) * 0.001F;
        speed_mps = 12.0F + 8.0F * sinf(kTwoPi * t / 60.0F);
        accel_x_mps2 = 8.0F * (kTwoPi / 60.0F) * cosf(kTwoPi * t / 60.0F);
        yaw_rate_dps = 10.0F * sinf(kTwoPi * t / 20.0F);
        soc_pct = 90.0F - t / 120.0F;  // 0.5 % per minute
        if (soc_pct < 20.0F) {
            soc_pct = 20.0F;
        }
        current_a = 5.0F + 1.5F * speed_mps;
        voltage_v = 44.0F + 0.05F * soc_pct - 0.02F * current_a;
        temperature_c = 25.0F + 0.4F * speed_mps;
    }
};

SensorModel g_model;
ecu::ProtectedReceiver<proto::ControllerCommand> g_command_rx;
ecu::ProtectedReceiver<proto::ControllerHeartbeat> g_heartbeat_rx;
proto::ControllerCommand g_last_command{};
bool g_have_command = false;
proto::e2e::CounterGenerator g_data_counter;
proto::e2e::CounterGenerator g_battery_counter;
proto::e2e::CounterGenerator g_heartbeat_counter;
std::uint32_t g_tx_data = 0;
std::uint32_t g_tx_battery = 0;
std::uint32_t g_tx_heartbeat = 0;

// Slots within the 100 ms cycle (in 10 ms ticks) for the 100 ms messages, so
// they do not all land in the same tick as a sensor sample.
constexpr std::uint32_t kBatterySlot = 3;
constexpr std::uint32_t kHeartbeatSlot = 6;

}  // namespace

void init(std::int64_t /*now_ms*/) {}

bool handle_frame(const can_frame& frame, std::int64_t now_ms)
{
    ecu::RxResult result{};
    switch (frame.id) {
    case proto::ControllerCommand::kId: {
        proto::ControllerCommand cmd{};
        {
            Locked l(shared().lock);
            result = g_command_rx.receive(frame.data, frame.dlc, cmd);
            if (ecu::accepted(result)) {
                shared().monitor.note_received(frame.id, now_ms);
                g_last_command = cmd;
                g_have_command = true;
            }
        }
        note_rx_result(result);
        return true;
    }
    case proto::ControllerHeartbeat::kId: {
        proto::ControllerHeartbeat hb{};
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
    g_model.step(now_ms);

    proto::SensorData data{};
    data.sensor_status = proto::SensorStatus::kOk;
    data.set_speed_phys(g_model.speed_mps);
    data.set_yaw_rate_phys(g_model.yaw_rate_dps);
    data.set_accel_x_phys(g_model.accel_x_mps2);
    if (transmit<proto::SensorData>(proto::e2e::protect(data, g_data_counter.next())) == 0) {
        ++g_tx_data;
    }

    constexpr std::uint32_t kTicksPer100ms = proto::BatteryStatus::kCycleMs / cfg::kBaseTickMs;
    if (tick % kTicksPer100ms == kBatterySlot) {
        proto::BatteryStatus battery{};
        battery.set_pack_voltage_phys(g_model.voltage_v);
        battery.set_pack_current_phys(g_model.current_a);
        battery.set_state_of_charge_phys(g_model.soc_pct);
        battery.set_temperature_phys(g_model.temperature_c);
        battery.low_voltage = g_model.soc_pct < 25.0F ? 1 : 0;
        battery.over_temperature = g_model.temperature_c > 60.0F ? 1 : 0;
        battery.charging = 0;
        if (transmit<proto::BatteryStatus>(proto::e2e::protect(battery, g_battery_counter.next())) == 0) {
            ++g_tx_battery;
        }
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
    in.critical_inputs_alive = m.all_alive();
    in.critical_inputs_lost = m.lost(proto::ControllerCommand::kId) || m.lost(proto::ControllerHeartbeat::kId);
    in.degraded_quality = g_command_rx.stats().consecutive_rejects >= cfg::kQualityFaultThreshold ||
                          g_heartbeat_rx.stats().consecutive_rejects >= cfg::kQualityFaultThreshold;
    in.can_failed = shared().can_sm.failed();
    in.bus_off = shared().can_sm.bus_off();
    return in;
}

void on_message_lost(std::uint32_t id)
{
    Locked l(shared().lock);
    if (id == proto::ControllerCommand::kId) {
        g_command_rx.reset_sequence();
    } else if (id == proto::ControllerHeartbeat::kId) {
        g_heartbeat_rx.reset_sequence();
    }
}

void on_node_state(proto::NodeState /*state*/) {}

void reset_stats()
{
    Locked l(shared().lock);
    g_command_rx.reset_stats();
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
    // The model and the tx counters belong to the TX thread; a torn read of a
    // diagnostic float is acceptable, a long critical section is not.
    proto::ControllerCommand cmd{};
    bool have_command = false;
    {
        Locked l(shared().lock);
        out.rx_count = 2;
        fill_rx(out.rx[0], proto::ControllerCommand::kName, g_command_rx.stats(), proto::ControllerCommand::kId,
                now_ms);
        fill_rx(out.rx[1], proto::ControllerHeartbeat::kName, g_heartbeat_rx.stats(),
                proto::ControllerHeartbeat::kId, now_ms);
        cmd = g_last_command;
        have_command = g_have_command;
    }
    std::snprintf(out.role_detail, sizeof(out.role_detail),
                  "tx data=%u battery=%u heartbeat=%u speed_cm_s=%d yaw_0.1deg_s=%d soc_0.5pct=%u "
                  "cmd: have=%d mode=%s torque_0.01Nm=%d enabled=%u",
                  static_cast<unsigned>(g_tx_data), static_cast<unsigned>(g_tx_battery),
                  static_cast<unsigned>(g_tx_heartbeat), static_cast<int>(g_model.speed_mps * 100.0F),
                  static_cast<int>(g_model.yaw_rate_dps * 10.0F), static_cast<unsigned>(g_model.soc_pct * 2.0F),
                  have_command ? 1 : 0, proto::to_string(cmd.mode), static_cast<int>(cmd.torque_cmd),
                  static_cast<unsigned>(cmd.actuators_enabled));
}

}  // namespace app::role

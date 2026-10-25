// Phase 1 smoke application: send the sensor ECU heartbeat, protected with the
// rolling counter and CRC from the generated protocol library, at its DBC cycle
// time. On native_sim the frames land on a Linux SocketCAN interface where
// `candump` and the Python reference implementation can check them; on
// nucleo_f446re the same source drives the on-chip bxCAN peripheral.
// Phase 2 replaces this file with the real ECU application.

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cstdint>
#include <cstring>

#include "protocol/protocol.hpp"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

namespace {

using Heartbeat = proto::SensorHeartbeat;

constexpr std::uint32_t kTxMailboxTimeoutMs = 10;
constexpr std::uint32_t kTxCompleteTimeoutMs = 100;

const device* const kCanDev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

// Periodic timer that paces the loop from absolute expiry points, so the period
// does not accumulate the time spent sending.
K_TIMER_DEFINE(period_timer, nullptr, nullptr);

// Transmit completion: can_send() with a callback returns as soon as the frame
// is in a TX mailbox; the callback reports the actual result. Each send carries
// a sequence number as user_data so a completion that arrives after its wait
// timed out (possible on bxCAN when nobody acknowledges) is recognised as stale
// instead of being credited to the next frame. The semaphore orders the writes
// in the callback before main's reads.
K_SEM_DEFINE(tx_done_sem, 0, 1);
int tx_result = 0;
std::uintptr_t tx_done_seq = 0;

void on_tx_done(const device* /*dev*/, int error, void* user_data)
{
    tx_result = error;
    tx_done_seq = reinterpret_cast<std::uintptr_t>(user_data);
    k_sem_give(&tx_done_sem);
}

// Zephyr ignores main()'s return value, so a fatal init error must stop the
// system explicitly: on native_sim k_panic() exits the process with status 1,
// on Cortex-M it halts in the fatal-error handler.
[[noreturn]] void fatal(const char* what, int err)
{
    LOG_ERR("%s (%d)", what, err);
    k_panic();
    CODE_UNREACHABLE;
}

// Zephyr reports a fifth state, error-warning (an error counter at or above 96).
// Per ISO 11898-1 that is still error-active, and the 2-bit DBC field has no
// room for it, so it is folded into kErrorActive; the error counters, not the
// state, show the warning level.
proto::CanState to_proto(can_state state)
{
    switch (state) {
    case CAN_STATE_ERROR_ACTIVE:
    case CAN_STATE_ERROR_WARNING:
        return proto::CanState::kErrorActive;
    case CAN_STATE_ERROR_PASSIVE:
        return proto::CanState::kErrorPassive;
    case CAN_STATE_BUS_OFF:
        return proto::CanState::kBusOff;
    case CAN_STATE_STOPPED:
    default:
        return proto::CanState::kStopped;
    }
}

// Build the heartbeat and protect it. can_state and uptime are live; node_state,
// bus_off_count and error_flags are constant placeholders until Phase 2's
// supervisor and CAN error-state machine own them.
Heartbeat::Frame make_heartbeat(proto::e2e::CounterGenerator& counter)
{
    can_state state = CAN_STATE_STOPPED;
    can_bus_err_cnt err_cnt{};
    (void)can_get_state(kCanDev, &state, &err_cnt);

    Heartbeat hb{};
    hb.node_state = proto::NodeState::kRunning;
    hb.uptime = k_uptime_get_32();
    hb.can_state = to_proto(state);
    hb.bus_off_count = 0;
    hb.error_flags = 0;
    return proto::e2e::protect(hb, counter.next());
}

// Send one frame and wait, with a bound, for the controller to confirm it.
// A NULL callback would make can_send() wait for completion with K_FOREVER,
// which on the SocketCAN back end never returns if the write is dropped.
bool send_frame(std::uint32_t id, const Heartbeat::Frame& payload)
{
    can_frame frame{};
    frame.id = id;
    frame.dlc = static_cast<std::uint8_t>(payload.size());
    frame.flags = 0;
    std::memcpy(frame.data, payload.data(), payload.size());

    static std::uintptr_t tx_seq = 0;
    ++tx_seq;
    k_sem_reset(&tx_done_sem);
    const int err = can_send(kCanDev, &frame, K_MSEC(kTxMailboxTimeoutMs), on_tx_done,
                             reinterpret_cast<void*>(tx_seq));
    if (err != 0) {
        LOG_WRN("can_send rejected the frame (%d)", err);
        return false;
    }
    // Wait for *this* frame's completion; a stale one from an earlier send is skipped.
    const std::int64_t deadline = k_uptime_get() + kTxCompleteTimeoutMs;
    for (;;) {
        const std::int64_t remaining = deadline - k_uptime_get();
        if (remaining <= 0 || k_sem_take(&tx_done_sem, K_MSEC(remaining)) != 0) {
            LOG_WRN("TX not confirmed within %u ms", kTxCompleteTimeoutMs);
            return false;
        }
        if (tx_done_seq == tx_seq) {
            break;
        }
    }
    if (tx_result != 0) {
        LOG_WRN("TX failed (%d)", tx_result);
        return false;
    }
    return true;
}

}  // namespace

int main()
{
    if (!device_is_ready(kCanDev)) {
        fatal("CAN device is not ready", -ENODEV);
    }

    const int err = can_start(kCanDev);
    if (err != 0 && err != -EALREADY) {
        fatal("can_start failed", err);
    }
    LOG_INF("CAN device %s started, sending %s (0x%03x) every %u ms",
            kCanDev->name, Heartbeat::kName, Heartbeat::kId, Heartbeat::kCycleMs);

    k_timer_start(&period_timer, K_MSEC(Heartbeat::kCycleMs), K_MSEC(Heartbeat::kCycleMs));

    proto::e2e::CounterGenerator counter;
    while (true) {
        const Heartbeat::Frame frame = make_heartbeat(counter);
        if (send_frame(Heartbeat::kId, frame)) {
            LOG_INF("sent %s counter=%u crc=0x%02x", Heartbeat::kName,
                    proto::e2e::counter_of(frame), frame[0]);
        }
        k_timer_status_sync(&period_timer);  // block until the next period boundary
    }
    return 0;
}

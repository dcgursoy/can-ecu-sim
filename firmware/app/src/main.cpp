// Phase 0 smoke application: prove the toolchain end to end by sending one CAN
// frame per second through the Zephyr CAN API. On native_sim the frame lands on a
// Linux SocketCAN interface where `candump` can see it; on nucleo_f446re the same
// source drives the on-chip bxCAN peripheral.

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <array>
#include <cstdint>
#include <cstring>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

namespace {

constexpr std::uint32_t kHelloFrameId = 0x100;
constexpr std::uint32_t kPeriodMs = 1000;
constexpr std::uint32_t kTxMailboxTimeoutMs = 10;
constexpr std::uint32_t kTxCompleteTimeoutMs = 100;

const device* const kCanDev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

// Periodic timer that paces the loop from absolute expiry points, so the period
// does not accumulate the time spent sending (a relative k_sleep after the send
// would drift by the send time plus one tick of rounding per iteration).
K_TIMER_DEFINE(period_timer, nullptr, nullptr);

// Transmit completion: can_send() with a callback returns as soon as the frame
// is in a TX mailbox; the callback reports the actual result. The semaphore
// orders the write of tx_result before main's read of it.
K_SEM_DEFINE(tx_done_sem, 0, 1);
int tx_result = 0;

void on_tx_done(const device* /*dev*/, int error, void* /*user_data*/)
{
    tx_result = error;
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

// Build the frame as a value: "HELLO" followed by a 24-bit frame counter.
can_frame make_hello_frame(std::uint32_t counter)
{
    can_frame frame{};
    frame.id = kHelloFrameId;
    frame.dlc = 8;
    frame.flags = 0;

    const std::array<std::uint8_t, 8> payload{
        'H', 'E', 'L', 'L', 'O',
        static_cast<std::uint8_t>(counter >> 16),
        static_cast<std::uint8_t>(counter >> 8),
        static_cast<std::uint8_t>(counter),
    };
    std::memcpy(frame.data, payload.data(), payload.size());
    return frame;
}

// Send one frame and wait, with a bound, for the controller to confirm it.
// A NULL callback would make can_send() wait for completion with K_FOREVER,
// which on the SocketCAN back end never returns if the write is dropped.
bool send_frame(const can_frame& frame)
{
    k_sem_reset(&tx_done_sem);
    const int err = can_send(kCanDev, &frame, K_MSEC(kTxMailboxTimeoutMs), on_tx_done, nullptr);
    if (err != 0) {
        LOG_WRN("can_send rejected the frame (%d)", err);
        return false;
    }
    if (k_sem_take(&tx_done_sem, K_MSEC(kTxCompleteTimeoutMs)) != 0) {
        LOG_WRN("TX not confirmed within %u ms", kTxCompleteTimeoutMs);
        return false;
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
    LOG_INF("CAN device %s started, sending 0x%03x every %u ms",
            kCanDev->name, kHelloFrameId, kPeriodMs);

    k_timer_start(&period_timer, K_MSEC(kPeriodMs), K_MSEC(kPeriodMs));

    std::uint32_t counter = 0;
    while (true) {
        const can_frame frame = make_hello_frame(counter);
        if (send_frame(frame)) {
            LOG_INF("sent id=0x%03x counter=%u", frame.id, counter);
        }
        ++counter;
        k_timer_status_sync(&period_timer);  // block until the next period boundary
    }
    return 0;
}

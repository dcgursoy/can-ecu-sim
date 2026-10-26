// Common application layer shared by both roles: the shared state and its lock,
// the HAL instances, time, liveness, transmit path and heartbeat assembly. The
// threads themselves live in ecu.cpp.
#pragma once

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

#include <cstddef>
#include <cstdint>

#include "app/config.hpp"
#include "app/role.hpp"
#include "ecu/can_error_sm.hpp"
#include "ecu/liveness.hpp"
#include "ecu/node_state.hpp"
#include "ecu/protected_rx.hpp"
#include "ecu/timeout_monitor.hpp"
#include "hal/can_bus.hpp"
#include "hal/watchdog.hpp"
#include "protocol/protocol.hpp"

namespace app {

enum class ThreadId : std::size_t { kRx = 0, kTx = 1, kSupervisor = 2, kCount = 3 };

/// Bits of the heartbeat's error_flags field (assignment documented in the DBC).
enum ErrorFlag : std::uint8_t {
    kCrcFaultSeen = 1U << 0,
    kCounterFaultSeen = 1U << 1,
    kRxTimeoutSeen = 1U << 2,
    kBusOffSeen = 1U << 3,
    kWatchdogWarning = 1U << 4,
    kTestHookActive = 1U << 5,
};

/// State touched by more than one thread. Guarded by `lock`; hold it only for
/// the few instructions it takes to read or update, never while logging or
/// calling into the driver.
struct Shared {
    k_spinlock lock{};
    ecu::TimeoutMonitor<role::kMonitoredCount> monitor{role::kMonitored};
    ecu::NodeStateMachine node{role::kNodePolicy};
    ecu::CanErrorStateMachine can_sm{cfg::kBusOffPolicy};
    ecu::Liveness<static_cast<std::size_t>(ThreadId::kCount)> liveness{cfg::kLivenessWindowMs};
    std::uint8_t error_flags = 0;
    std::uint32_t rx_frames = 0;
    std::uint32_t rx_unknown = 0;
    std::uint32_t rx_queue_hwm = 0;
    std::uint32_t echo_responses = 0;
    std::uint32_t test_commands = 0;
    std::uint32_t tx_sent = 0;
    std::uint32_t tx_tick_overruns = 0;
    std::uint32_t wdt_feeds = 0;
    std::uint32_t wdt_skipped = 0;
    std::uint32_t liveness_stale_mask = 0;
};

/// Scoped spinlock.
class Locked {
public:
    explicit Locked(k_spinlock& lock) : lock_(lock), key_(k_spin_lock(&lock)) {}
    ~Locked() { k_spin_unlock(&lock_, key_); }
    Locked(const Locked&) = delete;
    Locked& operator=(const Locked&) = delete;

private:
    k_spinlock& lock_;
    k_spinlock_key_t key_;
};

Shared& shared();
hal::CanBus& can();
hal::Watchdog& watchdog();

inline std::int64_t now_ms() { return k_uptime_get(); }

void check_in(ThreadId id);
void set_error_flag(ErrorFlag flag);

/// Account a receive result in the error flags (CRC and counter faults).
void note_rx_result(ecu::RxResult result);

/// Transmit through the HAL unless the CAN error state machine forbids it
/// (bus-off backoff/recovery, latched failure). Returns 0 or -errno.
int transmit(std::uint32_t id, const std::uint8_t* data, std::size_t dlc);

template <class Msg>
int transmit(const typename Msg::Frame& frame)
{
    return transmit(Msg::kId, frame.data(), Msg::kDlc);
}

/// This node's heartbeat from the shared state, protected with the next counter.
template <class Hb>
typename Hb::Frame make_heartbeat(proto::e2e::CounterGenerator& counter, std::int64_t now)
{
    Hb hb{};
    {
        Locked l(shared().lock);
        hb.node_state = shared().node.state();
        const std::uint32_t bus_off = shared().can_sm.counters().bus_off_events;
        hb.bus_off_count = static_cast<std::uint8_t>(bus_off > 63U ? 63U : bus_off);
        hb.error_flags = shared().error_flags;
    }
    hb.uptime = static_cast<std::uint32_t>(now);
    hb.can_state = can().state();
    return proto::e2e::protect(hb, counter.next());
}

/// Initialise the HAL, queues and threads. Returns 0 or -errno.
int start();

}  // namespace app

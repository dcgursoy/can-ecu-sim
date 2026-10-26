// Common application layer: queues, the three application threads and the
// supervisor logic. See DESIGN.md section 11 for the priorities and periods.
//
//   can_rx (prio 3)      drains the driver's frame queue, validates and
//                        dispatches: echo, test control, role messages
//   can_tx (prio 4)      10 ms base tick; the role emits whatever is due
//   supervisor (prio 5)  5 ms tick: CAN controller events and bus-off
//                        recovery, message timeouts, node state, watchdog
#include "app/ecu.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cerrno>
#include <cstring>

#include "app/test_control.hpp"

extern "C" void alloc_guard_arm(void);

LOG_MODULE_REGISTER(ecu, LOG_LEVEL_INF);

namespace app {

namespace {

// --- shared objects ------------------------------------------------------------------
Shared g_shared;
hal::CanBus g_can;
hal::Watchdog g_watchdog;

// --- queues --------------------------------------------------------------------------
// Frames from the CAN driver. Sized in frames; the driver drops when it is full.
alignas(4) char g_rx_queue_buffer[cfg::kRxQueueDepth * sizeof(can_frame)];
k_msgq g_rx_queue;

// Controller state changes, posted by the HAL listener from driver or work
// context and consumed by the supervisor.
struct CanEvent {
    proto::CanState state;
    std::uint8_t tec;
    std::uint8_t rec;
    std::int64_t t_ms;
};
constexpr std::size_t kCanEventDepth = 8;
alignas(8) char g_can_event_buffer[kCanEventDepth * sizeof(CanEvent)];
k_msgq g_can_event_queue;
std::uint32_t g_can_events_dropped = 0;

// --- threads -------------------------------------------------------------------------
K_THREAD_STACK_DEFINE(g_rx_stack, cfg::kThreadStackSize);
K_THREAD_STACK_DEFINE(g_tx_stack, cfg::kThreadStackSize);
K_THREAD_STACK_DEFINE(g_supervisor_stack, cfg::kThreadStackSize);
k_thread g_rx_thread;
k_thread g_tx_thread;
k_thread g_supervisor_thread;
K_TIMER_DEFINE(g_tx_timer, nullptr, nullptr);
K_TIMER_DEFINE(g_supervisor_timer, nullptr, nullptr);

proto::e2e::CounterGenerator g_heartbeat_counter;  // used by the role through make_heartbeat()

void on_can_state(proto::CanState state, std::uint8_t tec, std::uint8_t rec, void* /*user*/)
{
    const CanEvent ev{state, tec, rec, now_ms()};
    if (k_msgq_put(&g_can_event_queue, &ev, K_NO_WAIT) != 0) {
        ++g_can_events_dropped;
    }
}

// --- RX thread -----------------------------------------------------------------------
void handle_echo(const can_frame& frame, std::uint32_t t_rx_cycles)
{
    proto::EchoRequest request{};
    if (!proto::EchoRequest::from_buffer(frame.data, frame.dlc, request)) {
        return;
    }
    if (request.target_node != proto::Node::kAll && request.target_node != role::kNode) {
        return;
    }
    role::EchoResponse response{};
    response.seq = request.seq;
    response.payload = request.payload;
    const std::uint32_t turnaround_us = k_cyc_to_us_floor32(k_cycle_get_32() - t_rx_cycles);
    response.turnaround = static_cast<std::uint16_t>(turnaround_us > 65535U ? 65535U : turnaround_us);
    if (transmit<role::EchoResponse>(response.to_frame()) == 0) {
        Locked l(g_shared.lock);
        ++g_shared.echo_responses;
    }
}

void rx_thread_fn(void*, void*, void*)
{
    can_frame frame{};
    for (;;) {
        check_in(ThreadId::kRx);
        // The timeout keeps the liveness check-in flowing when the bus is silent.
        if (k_msgq_get(&g_rx_queue, &frame, K_MSEC(cfg::kLivenessWindowMs / 4)) != 0) {
            continue;
        }
        const std::uint32_t t_rx_cycles = k_cycle_get_32();
        const std::int64_t now = now_ms();
        {
            Locked l(g_shared.lock);
            ++g_shared.rx_frames;
            const std::uint32_t queued = k_msgq_num_used_get(&g_rx_queue) + 1U;
            if (queued > g_shared.rx_queue_hwm) {
                g_shared.rx_queue_hwm = queued;
            }
        }
        // Standard data frames only: no extended ids, remote frames or CAN FD.
        if ((frame.flags & (CAN_FRAME_IDE | CAN_FRAME_RTR | CAN_FRAME_FDF)) != 0U || frame.dlc > 8U) {
            Locked l(g_shared.lock);
            ++g_shared.rx_unknown;
            continue;
        }
        switch (frame.id) {
        case proto::EchoRequest::kId:
            handle_echo(frame, t_rx_cycles);
            break;
        case proto::TestControl::kId:
            if (IS_ENABLED(CONFIG_APP_TEST_HOOKS)) {
                test_control::handle(frame, now);
            } else {
                Locked l(g_shared.lock);
                ++g_shared.rx_unknown;
            }
            break;
        default:
            if (!role::handle_frame(frame, now)) {
                Locked l(g_shared.lock);
                ++g_shared.rx_unknown;
            }
            break;
        }
    }
}

// --- TX thread -----------------------------------------------------------------------
void tx_thread_fn(void*, void*, void*)
{
    check_in(ThreadId::kTx);  // before the first tick, so the supervisor never sees a never-started thread
    k_timer_start(&g_tx_timer, K_MSEC(cfg::kBaseTickMs), K_MSEC(cfg::kBaseTickMs));
    std::uint32_t tick = 0;
    for (;;) {
        const std::uint32_t expiries = k_timer_status_sync(&g_tx_timer);
        if (expiries > 1U) {
            Locked l(g_shared.lock);
            g_shared.tx_tick_overruns += expiries - 1U;
        }
        check_in(ThreadId::kTx);
        role::periodic_tx(tick, now_ms());
        ++tick;
    }
}

// --- supervisor thread ---------------------------------------------------------------
void process_can_events(std::int64_t now)
{
    CanEvent ev{};
    while (k_msgq_get(&g_can_event_queue, &ev, K_NO_WAIT) == 0) {
        ecu::CanErrorStateMachine::Phase before{};
        ecu::CanErrorStateMachine::Phase after{};
        proto::CanState previous{};
        {
            Locked l(g_shared.lock);
            previous = g_shared.can_sm.controller_state();
            before = g_shared.can_sm.phase();
            g_shared.can_sm.on_controller_state(ev.state, ev.t_ms);
            after = g_shared.can_sm.phase();
            if (ev.state == proto::CanState::kBusOff) {
                g_shared.error_flags |= kBusOffSeen;
            }
        }
        LOG_WRN("can: %s -> %s (tec=%u rec=%u) phase %s -> %s", proto::to_string(previous),
                proto::to_string(ev.state), ev.tec, ev.rec, ecu::CanErrorStateMachine::to_string(before),
                ecu::CanErrorStateMachine::to_string(after));
    }

    // Level-triggered input: bxCAN raises its status interrupt only when an error
    // flag is set, never when it clears, so the return to error-active after a
    // recovery (and passive -> active) would never arrive as an event. One
    // can_get_state() per tick (a register read) closes that gap on every target.
    const proto::CanState polled = g_can.state();  // outside the lock: calls the driver
    ecu::CanErrorStateMachine::Phase before{};
    ecu::CanErrorStateMachine::Phase after{};
    proto::CanState previous{};
    bool changed = false;
    {
        Locked l(g_shared.lock);
        previous = g_shared.can_sm.controller_state();
        before = g_shared.can_sm.phase();
        g_shared.can_sm.poll_controller_state(polled, now);
        after = g_shared.can_sm.phase();
        changed = after != before || g_shared.can_sm.controller_state() != previous;
        if (changed && polled == proto::CanState::kBusOff) {
            g_shared.error_flags |= kBusOffSeen;
        }
    }
    if (changed) {
        LOG_WRN("can (polled): %s -> %s phase %s -> %s", proto::to_string(previous), proto::to_string(polled),
                ecu::CanErrorStateMachine::to_string(before), ecu::CanErrorStateMachine::to_string(after));
    }
}

void run_bus_off_recovery(std::int64_t now)
{
    ecu::CanErrorStateMachine::Action action{};
    std::uint32_t attempt = 0;
    std::uint32_t backoff = 0;
    {
        Locked l(g_shared.lock);
        backoff = g_shared.can_sm.current_backoff_ms();
        action = g_shared.can_sm.tick(now);
        attempt = g_shared.can_sm.counters().recovery_attempts;
    }
    if (action != ecu::CanErrorStateMachine::Action::kStartRecovery) {
        return;
    }
    const int err = g_can.recover();
    bool failed_now = false;
    {
        Locked l(g_shared.lock);
        g_shared.can_sm.on_recovery_request_result(err == 0, now);
        failed_now = g_shared.can_sm.failed();
    }
    LOG_WRN("bus-off recovery attempt %u after %u ms backoff: %s (%d)", attempt, backoff,
            err == 0 ? "requested" : "refused", err);
    if (failed_now) {
        LOG_ERR("bus-off recovery exhausted after %u attempts: node FAULT", attempt);
    }
}

void run_timeouts(std::int64_t now)
{
    std::array<ecu::MonitoredMessage, role::kMonitoredCount> lost{};
    std::size_t n = 0;
    {
        Locked l(g_shared.lock);
        g_shared.monitor.tick(now, [&](const auto& entry) {
            if (n < lost.size()) {
                lost[n++] = entry.cfg;
            }
        });
        if (n > 0) {
            g_shared.error_flags |= kRxTimeoutSeen;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        role::on_message_lost(lost[i].id);
        LOG_WRN("timeout: %s not received for %u ms", lost[i].name, lost[i].timeout_ms);
    }
}

void run_node_state(std::int64_t now)
{
    const ecu::NodeInputs in = role::inputs(now);
    bool changed = false;
    proto::NodeState state{};
    const char* reason = "";
    proto::NodeState before{};
    {
        Locked l(g_shared.lock);
        before = g_shared.node.state();
        changed = g_shared.node.update(in, now);
        state = g_shared.node.state();
        reason = g_shared.node.last_reason();
    }
    if (changed) {
        LOG_INF("node_state: %s -> %s (%s)", proto::to_string(before), proto::to_string(state), reason);
        role::on_node_state(state);
    }
}

void run_watchdog(std::int64_t now)
{
    bool feed = false;
    std::uint32_t stale = 0;
    {
        Locked l(g_shared.lock);
        stale = g_shared.liveness.stale_mask(now);
        g_shared.liveness_stale_mask = stale;
        feed = stale == 0U;
        if (feed) {
            ++g_shared.wdt_feeds;
        } else {
            ++g_shared.wdt_skipped;
            g_shared.error_flags |= kWatchdogWarning;
        }
    }
    if (feed) {
        (void)g_watchdog.feed();
    } else {
        static std::int64_t last_warning = -10000;
        if (now - last_warning >= 1000) {
            last_warning = now;
            LOG_WRN("watchdog not fed: stale threads 0x%x", stale);
        }
    }
}

void supervisor_thread_fn(void*, void*, void*)
{
    {
        Locked l(g_shared.lock);
        g_shared.monitor.start(now_ms());
        g_shared.node.start(now_ms());
    }
    check_in(ThreadId::kSupervisor);
    k_timer_start(&g_supervisor_timer, K_MSEC(cfg::kSupervisorTickMs), K_MSEC(cfg::kSupervisorTickMs));
    for (;;) {
        k_timer_status_sync(&g_supervisor_timer);
        const std::int64_t now = now_ms();
        check_in(ThreadId::kSupervisor);
        process_can_events(now);
        run_bus_off_recovery(now);
        run_timeouts(now);
        run_node_state(now);
        run_watchdog(now);
    }
}

}  // namespace

// --- accessors -----------------------------------------------------------------------
Shared& shared() { return g_shared; }
hal::CanBus& can() { return g_can; }
hal::Watchdog& watchdog() { return g_watchdog; }

void check_in(ThreadId id)
{
    Locked l(g_shared.lock);
    g_shared.liveness.check_in(static_cast<std::size_t>(id), now_ms());
}

void set_error_flag(ErrorFlag flag)
{
    Locked l(g_shared.lock);
    g_shared.error_flags |= flag;
}

void note_rx_result(ecu::RxResult result)
{
    switch (result) {
    case ecu::RxResult::kRejectedCrc:
    case ecu::RxResult::kRejectedDlc:
        set_error_flag(kCrcFaultSeen);
        break;
    case ecu::RxResult::kRejectedRepeated:
    case ecu::RxResult::kRejectedOutOfSequence:
        set_error_flag(kCounterFaultSeen);
        break;
    case ecu::RxResult::kAccepted:
    case ecu::RxResult::kAcceptedWithLoss:
        break;
    }
}

int transmit(std::uint32_t id, const std::uint8_t* data, std::size_t dlc)
{
    {
        Locked l(g_shared.lock);
        if (!g_shared.can_sm.tx_allowed()) {
            return -ENETDOWN;
        }
    }
    can_frame frame{};
    frame.id = id;
    frame.dlc = static_cast<std::uint8_t>(dlc);
    frame.flags = 0;
    std::memcpy(frame.data, data, dlc);
    const int err = g_can.send(frame);
    if (err == 0) {
        Locked l(g_shared.lock);
        ++g_shared.tx_sent;
    }
    return err;
}

int start()
{
    k_msgq_init(&g_rx_queue, g_rx_queue_buffer, sizeof(can_frame), cfg::kRxQueueDepth);
    k_msgq_init(&g_can_event_queue, g_can_event_buffer, sizeof(CanEvent), kCanEventDepth);

    int err = g_can.init(&g_rx_queue, &on_can_state, nullptr);
    if (err != 0) {
        LOG_ERR("CAN init failed (%d)", err);
        return err;
    }
    err = g_watchdog.init(cfg::kWatchdogTimeoutMs);
    if (err != 0) {
        LOG_ERR("watchdog init failed (%d)", err);
        return err;
    }
    role::init(now_ms());
    err = g_can.start();
    if (err != 0) {
        LOG_ERR("CAN start failed (%d)", err);
        return err;
    }

    k_tid_t tid = k_thread_create(&g_rx_thread, g_rx_stack, K_THREAD_STACK_SIZEOF(g_rx_stack), rx_thread_fn,
                                  nullptr, nullptr, nullptr, cfg::kRxThreadPriority, 0, K_NO_WAIT);
    k_thread_name_set(tid, "can_rx");
    tid = k_thread_create(&g_tx_thread, g_tx_stack, K_THREAD_STACK_SIZEOF(g_tx_stack), tx_thread_fn, nullptr,
                          nullptr, nullptr, cfg::kTxThreadPriority, 0, K_NO_WAIT);
    k_thread_name_set(tid, "can_tx");
    tid = k_thread_create(&g_supervisor_thread, g_supervisor_stack, K_THREAD_STACK_SIZEOF(g_supervisor_stack),
                          supervisor_thread_fn, nullptr, nullptr, nullptr, cfg::kSupervisorThreadPriority, 0,
                          K_NO_WAIT);
    k_thread_name_set(tid, "supervisor");

    LOG_INF("ecu role=%s node=%u rx_prio=%d tx_prio=%d sup_prio=%d rx_queue=%u sup_tick=%u ms wdt=%u ms",
            role::kName, static_cast<unsigned>(role::kNode), cfg::kRxThreadPriority, cfg::kTxThreadPriority,
            cfg::kSupervisorThreadPriority, cfg::kRxQueueDepth, cfg::kSupervisorTickMs, cfg::kWatchdogTimeoutMs);
    alloc_guard_arm();  // from here on any allocation is a fatal error (src/alloc_guard.c)
    return 0;
}

}  // namespace app

// --- C interface for the shell -----------------------------------------------------------
extern "C" void ecu_status_get(struct ecu_status* out)
{
    using namespace app;
    std::memset(out, 0, sizeof(*out));
    const std::int64_t now = now_ms();
    out->role = role::kName;
    out->test_hooks = IS_ENABLED(CONFIG_APP_TEST_HOOKS) ? 1 : 0;
    out->uptime_ms = now;
    {
        Locked l(g_shared.lock);
        out->node_state = proto::to_string(g_shared.node.state());
        out->node_reason = g_shared.node.last_reason();
        out->node_state_age_ms = now - g_shared.node.entered_at_ms();
        out->safe_state_entries = g_shared.node.safe_state_entries();
        out->error_flags = g_shared.error_flags;
        out->can_phase = ecu::CanErrorStateMachine::to_string(g_shared.can_sm.phase());
        const auto& c = g_shared.can_sm.counters();
        out->bus_off_events = c.bus_off_events;
        out->passive_events = c.passive_events;
        out->recovery_attempts = c.recovery_attempts;
        out->recoveries_ok = c.recoveries_ok;
        out->recovery_failures = c.recovery_failures;
        out->last_recovery_duration_ms = c.last_recovery_duration_ms;
        out->current_backoff_ms = g_shared.can_sm.current_backoff_ms();
        out->tx_sent = g_shared.tx_sent;
        out->tx_tick_overruns = g_shared.tx_tick_overruns;
        out->rx_frames = g_shared.rx_frames;
        out->rx_unknown = g_shared.rx_unknown;
        out->rx_queue_hwm = g_shared.rx_queue_hwm;
        out->echo_responses = g_shared.echo_responses;
        out->test_commands = g_shared.test_commands;
        out->wdt_feeds = g_shared.wdt_feeds;
        out->wdt_skipped = g_shared.wdt_skipped;
        out->liveness_stale_mask = g_shared.liveness_stale_mask;
    }
    out->rx_queue_depth = cfg::kRxQueueDepth;
    out->rx_dropped = g_can.rx_dropped();
    out->can_events_dropped = g_can_events_dropped;
    out->can_state = proto::to_string(g_can.state());
    out->tec = g_can.tec();
    out->rec = g_can.rec();
    out->manual_recovery = g_can.manual_recovery_supported() ? 1 : 0;
    out->simulation_active = g_can.simulation_active() ? 1 : 0;
    const hal::CanBus::TxStats tx = g_can.tx_stats();
    out->tx_completed_ok = tx.completed_ok;
    out->tx_completed_error = tx.completed_error;
    out->tx_busy = tx.busy;
    out->tx_rejected = tx.rejected;
    out->tx_blocked_bus_off = tx.blocked_bus_off;
    out->wdt_available = g_watchdog.available() ? 1 : 0;
    out->wdt_timeout_ms = g_watchdog.timeout_ms();
    role::fill_status(*out, now);
}

extern "C" void ecu_reset_faults(void)
{
    using namespace app;
    const std::int64_t now = now_ms();
    {
        Locked l(g_shared.lock);
        g_shared.can_sm.reset(now);
        g_shared.node.reset(now);
        g_shared.error_flags = 0;
        g_shared.wdt_skipped = 0;
    }
    role::reset_stats();
}

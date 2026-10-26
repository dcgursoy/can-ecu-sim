#include "hal/can_bus.hpp"

#include <zephyr/logging/log.h>

#include <cerrno>

LOG_MODULE_REGISTER(hal_can, LOG_LEVEL_INF);

namespace hal {

CanBus* CanBus::instance_ = nullptr;

int CanBus::init(k_msgq* rx_queue, StateListener listener, void* listener_user)
{
    instance_ = this;
    rx_queue_ = rx_queue;
    listener_ = listener;
    listener_user_ = listener_user;
    dev_ = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!device_is_ready(dev_)) {
        return -ENODEV;
    }

    // Bus-off recovery is the application's decision where the controller
    // allows it (bxCAN: manual recovery disables automatic ABOM). The SocketCAN
    // driver has no such mode; there the simulated controller owns recovery.
    can_mode_t capabilities = 0;
    (void)can_get_capabilities(dev_, &capabilities);
    manual_recovery_ = (capabilities & CAN_MODE_MANUAL_RECOVERY) != 0U;
    if (manual_recovery_) {
        const int err = can_set_mode(dev_, CAN_MODE_MANUAL_RECOVERY);
        if (err != 0) {
            LOG_WRN("manual recovery mode rejected (%d), using automatic recovery", err);
            manual_recovery_ = false;
        }
    }

    // Accept every standard data frame; the application dispatches by identifier.
    // The callback runs in driver context (a thread on native_sim, the RX
    // interrupt on bxCAN): it only queues the frame and counts when it cannot.
    can_filter filter{};
    filter.id = 0;
    filter.mask = 0;
    filter.flags = 0;
    const int filter_id = can_add_rx_filter(dev_, &CanBus::rx_cb, this, &filter);
    if (filter_id < 0) {
        return filter_id;
    }
    can_set_state_change_callback(dev_, &CanBus::driver_state_cb, this);

#ifdef CONFIG_APP_TEST_HOOKS
    k_work_init_delayable(&sim_recover_work_, &CanBus::sim_recover_work_fn);
#endif
    LOG_INF("CAN %s ready (manual recovery %s, test hooks %s)", dev_->name,
            manual_recovery_ ? "yes" : "no", IS_ENABLED(CONFIG_APP_TEST_HOOKS) ? "on" : "off");
    return 0;
}

int CanBus::start()
{
    const int err = can_start(dev_);
    return err == -EALREADY ? 0 : err;
}

void CanBus::rx_cb(const device* /*dev*/, can_frame* frame, void* user)
{
    auto* self = static_cast<CanBus*>(user);
    if (k_msgq_put(self->rx_queue_, frame, K_NO_WAIT) != 0) {
        const k_spinlock_key_t key = k_spin_lock(&self->lock_);
        ++self->rx_dropped_;
        k_spin_unlock(&self->lock_, key);
    }
}

int CanBus::send(const can_frame& frame)
{
#ifdef CONFIG_APP_TEST_HOOKS
    {
        const k_spinlock_key_t key = k_spin_lock(&lock_);
        const bool blocked = sim_active_ && sim_model_.bus_off();
        if (blocked) {
            ++tx_.blocked_bus_off;
        }
        k_spin_unlock(&lock_, key);
        if (blocked) {
            return -ENETUNREACH;  // what a real bus-off controller does: nothing leaves the node
        }
    }
#endif
    const int err = can_send(dev_, &frame, K_MSEC(kMailboxTimeoutMs), &CanBus::tx_done_cb, this);
    const k_spinlock_key_t key = k_spin_lock(&lock_);
    if (err == 0) {
        ++tx_.submitted;
    } else if (err == -EAGAIN) {
        ++tx_.busy;
    } else {
        ++tx_.rejected;
    }
    k_spin_unlock(&lock_, key);
    return err;
}

int CanBus::recover()
{
#ifdef CONFIG_APP_TEST_HOOKS
    {
        const k_spinlock_key_t key = k_spin_lock(&lock_);
        const bool simulated = sim_active_ && sim_model_.bus_off();
        k_spin_unlock(&lock_, key);
        if (simulated) {
            k_work_schedule(&sim_recover_work_, K_MSEC(kSimRecoveryDelayMs));
            return 0;
        }
    }
#endif
    if (!manual_recovery_) {
        return -ENOTSUP;
    }
    // 0: the recovery sequence was started (or the controller is already out of
    // bus-off). bxCAN never signals its completion; the supervisor watches
    // state() for the return to error-active. -EAGAIN here is the driver's own
    // request-handshake timing out, a real refusal.
    return can_recover(dev_, K_NO_WAIT);
}

proto::CanState CanBus::state() const
{
#ifdef CONFIG_APP_TEST_HOOKS
    ecu::ErrorCounterModel model;
    if (sim_snapshot(model)) {
        return model.state();
    }
#endif
    can_state s = CAN_STATE_STOPPED;
    can_bus_err_cnt cnt{};
    if (can_get_state(dev_, &s, &cnt) != 0) {
        return proto::CanState::kStopped;
    }
    return to_proto(s);
}

std::uint8_t CanBus::tec() const
{
#ifdef CONFIG_APP_TEST_HOOKS
    ecu::ErrorCounterModel model;
    if (sim_snapshot(model)) {
        return model.tec();
    }
#endif
    can_state s = CAN_STATE_STOPPED;
    can_bus_err_cnt cnt{};
    return can_get_state(dev_, &s, &cnt) == 0 ? cnt.tx_err_cnt : 0;
}

std::uint8_t CanBus::rec() const
{
#ifdef CONFIG_APP_TEST_HOOKS
    ecu::ErrorCounterModel model;
    if (sim_snapshot(model)) {
        return model.rec();
    }
#endif
    can_state s = CAN_STATE_STOPPED;
    can_bus_err_cnt cnt{};
    return can_get_state(dev_, &s, &cnt) == 0 ? cnt.rx_err_cnt : 0;
}

bool CanBus::simulation_active() const
{
    const k_spinlock_key_t key = k_spin_lock(&lock_);
    const bool active = sim_active_;
    k_spin_unlock(&lock_, key);
    return active;
}

CanBus::TxStats CanBus::tx_stats() const
{
    const k_spinlock_key_t key = k_spin_lock(&lock_);
    const TxStats copy = tx_;
    k_spin_unlock(&lock_, key);
    return copy;
}

std::uint32_t CanBus::rx_dropped() const
{
    const k_spinlock_key_t key = k_spin_lock(&lock_);
    const std::uint32_t dropped = rx_dropped_;
    k_spin_unlock(&lock_, key);
    return dropped;
}

void CanBus::tx_done_cb(const device* /*dev*/, int error, void* user)
{
    auto* self = static_cast<CanBus*>(user);
    bool sim_active = false;
    {
        const k_spinlock_key_t key = k_spin_lock(&self->lock_);
        if (error == 0) {
            ++self->tx_.completed_ok;
        } else {
            ++self->tx_.completed_error;
        }
        sim_active = self->sim_active_;
        k_spin_unlock(&self->lock_, key);
    }
#ifdef CONFIG_APP_TEST_HOOKS
    // Real transmit results also drive the simulated counters, so an injected
    // error burst heals the way a real controller's would (TEC -1 per success).
    if (sim_active) {
        self->sim_apply(error == 0 ? ecu::ErrorCounterModel::Event::kTxSuccess : ecu::ErrorCounterModel::Event::kTxError, 1);
    }
#else
    (void)sim_active;
#endif
}

void CanBus::driver_state_cb(const device* /*dev*/, can_state state, can_bus_err_cnt err_cnt, void* user)
{
    auto* self = static_cast<CanBus*>(user);
    if (self->simulation_active()) {
        return;  // the simulated controller shadows the driver's (constant) state
    }
    self->notify(to_proto(state), err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);
}

proto::CanState CanBus::to_proto(can_state state)
{
    // Error-warning (a counter at or above 96) is still error-active per ISO
    // 11898-1 and the 2-bit DBC field has no room for it; the counters show it.
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

void CanBus::notify(proto::CanState state, std::uint8_t tec, std::uint8_t rec)
{
    if (listener_ != nullptr) {
        listener_(state, tec, rec, listener_user_);
    }
}

#ifdef CONFIG_APP_TEST_HOOKS

bool CanBus::sim_snapshot(ecu::ErrorCounterModel& out) const
{
    const k_spinlock_key_t key = k_spin_lock(&lock_);
    const bool active = sim_active_;
    out = sim_model_;
    k_spin_unlock(&lock_, key);
    return active;
}

void CanBus::sim_inject(ecu::ErrorCounterModel::Event event, unsigned count)
{
    sim_apply(event, count);
}

void CanBus::sim_force_bus_off()
{
    // 32 transmit errors take TEC from 0 past 255; fewer if it is already up.
    sim_apply(ecu::ErrorCounterModel::Event::kTxError, 32);
}

void CanBus::sim_apply(ecu::ErrorCounterModel::Event event, unsigned count)
{
    // Apply under the lock, report state changes outside it (the listener may
    // post to a queue; k_msgq_put with K_NO_WAIT is safe from any context).
    bool changed = false;
    proto::CanState state = proto::CanState::kErrorActive;
    std::uint8_t tec = 0;
    std::uint8_t rec = 0;
    {
        const k_spinlock_key_t key = k_spin_lock(&lock_);
        sim_active_ = true;  // from the first injected event on, the model is the controller
        for (unsigned i = 0; i < count; ++i) {
            sim_model_.apply(event);
        }
        state = sim_model_.state();
        tec = sim_model_.tec();
        rec = sim_model_.rec();
        changed = state != sim_last_state_;
        sim_last_state_ = state;
        k_spin_unlock(&lock_, key);
    }
    if (changed) {
        notify(state, tec, rec);
    }
}

void CanBus::sim_recover_work_fn(k_work* /*work*/)
{
    CanBus* self = instance_;
    if (self == nullptr) {
        return;
    }
    {
        const k_spinlock_key_t key = k_spin_lock(&self->lock_);
        self->sim_model_.recover();
        self->sim_last_state_ = self->sim_model_.state();
        k_spin_unlock(&self->lock_, key);
    }
    self->notify(proto::CanState::kErrorActive, 0, 0);
}

#endif  // CONFIG_APP_TEST_HOOKS

}  // namespace hal

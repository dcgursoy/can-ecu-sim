// Hardware abstraction of the CAN controller.
//
// Everything above this layer (protocol, state machines, roles) sees one API on
// both targets: frames in through a message queue, frames out through send(),
// controller state through a listener plus a pollable state(), and recovery
// through recover(). Below it:
//   nucleo_f446re  Zephyr bxCAN driver; state changes into worse states come
//                  from the peripheral's status interrupt, improvements are
//                  seen by polling state(); recover() is can_recover() in
//                  manual recovery mode
//   native_sim     Zephyr SocketCAN driver, which never reports an error state
//                  because a virtual bus has no electrical errors. With
//                  CONFIG_APP_TEST_HOOKS the HAL carries a simulated controller
//                  (ISO 11898-1 error counters, ecu::ErrorCounterModel) that
//                  test commands feed; its state changes travel through the same
//                  listener, bus-off blocks send() like the real thing, and
//                  recover() completes after a modelled 128 x 11-bit recovery
//                  delay, a fixed 3 ms (a virtual bus has no bit rate; the 1408
//                  bit times take 2.8 ms at 500 kbit/s). On the Nucleo the
//                  bxCAN peripheral performs the real sequence itself.
#pragma once

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "ecu/error_counter_model.hpp"
#include "protocol/messages.hpp"

namespace hal {

class CanBus {
public:
    using StateListener = void (*)(proto::CanState state, std::uint8_t tec, std::uint8_t rec, void* user);

    struct TxStats {
        std::uint32_t submitted = 0;        ///< accepted by the driver
        std::uint32_t completed_ok = 0;     ///< confirmed on the bus
        std::uint32_t completed_error = 0;  ///< driver reported a transmit error
        std::uint32_t busy = 0;             ///< no free mailbox (-EAGAIN)
        std::uint32_t rejected = 0;         ///< other driver errors
        std::uint32_t blocked_bus_off = 0;  ///< dropped because the (simulated) controller is bus-off
    };

    /// Bind to the chosen CAN controller, route all standard frames to `rx_queue`,
    /// install the state listener. Must be called before start().
    int init(k_msgq* rx_queue, StateListener listener, void* listener_user);
    int start();

    /// Bounded wait for a free TX mailbox. The SocketCAN driver has a single
    /// mailbox that is busy until the kernel echoes the frame back (about a
    /// millisecond of its polling RX thread), so two frames in one tick need a
    /// short wait; bxCAN has three mailboxes and rarely blocks at all.
    static constexpr std::uint32_t kMailboxTimeoutMs = 5;

    /// Transmit; waits up to kMailboxTimeoutMs for a mailbox, never for
    /// completion (that is counted in tx_stats() by the driver callback).
    int send(const can_frame& frame);

    /// Ask the controller to leave bus-off. Returns 0 if the request was accepted
    /// (completion is observed through state(), see the supervisor), -ENOTSUP if
    /// neither the driver nor the simulation can do it.
    int recover();

    /// Current controller state (driver register read, or the simulated model).
    proto::CanState state() const;
    std::uint8_t tec() const;
    std::uint8_t rec() const;
    bool manual_recovery_supported() const { return manual_recovery_; }
    bool simulation_active() const;
    TxStats tx_stats() const;
    /// Frames the driver could not hand over because the application queue was full.
    std::uint32_t rx_dropped() const;
    const device* device_handle() const { return dev_; }

#ifdef CONFIG_APP_TEST_HOOKS
    /// Feed `count` error events into the simulated controller.
    void sim_inject(ecu::ErrorCounterModel::Event event, unsigned count);
    /// Drive the simulated controller straight into bus-off.
    void sim_force_bus_off();
#endif

private:
    static void rx_cb(const device* dev, can_frame* frame, void* user);
    static void tx_done_cb(const device* dev, int error, void* user);
    static void driver_state_cb(const device* dev, can_state state, can_bus_err_cnt err_cnt, void* user);
    static proto::CanState to_proto(can_state state);
    void notify(proto::CanState state, std::uint8_t tec, std::uint8_t rec);

#ifdef CONFIG_APP_TEST_HOOKS
    static constexpr std::uint32_t kSimRecoveryDelayMs = 3;  // models 128 x 11 = 1408 recessive bit times
    static void sim_recover_work_fn(k_work* work);
    void sim_apply(ecu::ErrorCounterModel::Event event, unsigned count);
    /// Consistent copy of the simulated controller; returns false when the
    /// simulation is not active (then the real driver is the source of truth).
    bool sim_snapshot(ecu::ErrorCounterModel& out) const;
    k_work_delayable sim_recover_work_{};
    ecu::ErrorCounterModel sim_model_{};
    proto::CanState sim_last_state_ = proto::CanState::kErrorActive;
#endif

    const device* dev_ = nullptr;
    k_msgq* rx_queue_ = nullptr;
    StateListener listener_ = nullptr;
    void* listener_user_ = nullptr;
    bool manual_recovery_ = false;
    bool sim_active_ = false;           // guarded by lock_
    std::uint32_t rx_dropped_ = 0;      // guarded by lock_
    TxStats tx_{};                      // guarded by lock_
    mutable k_spinlock lock_{};
    static CanBus* instance_;
};

}  // namespace hal

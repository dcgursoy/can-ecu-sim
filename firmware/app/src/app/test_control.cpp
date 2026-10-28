#include "app/test_control.hpp"

#include <zephyr/logging/log.h>

#include "app/ecu.hpp"

LOG_MODULE_REGISTER(test_ctrl, LOG_LEVEL_INF);

namespace app::test_control {

#ifdef CONFIG_APP_TEST_HOOKS

// Every case updates the shared state in its own Locked scope and logs after
// it: the spinlock masks interrupts, and in immediate log mode LOG_* formats
// and writes to the console (and takes the shell's semaphore) in the caller.

void handle(const can_frame& frame, std::int64_t now_ms)
{
    proto::TestControl tc{};
    if (!proto::TestControl::from_buffer(frame.data, frame.dlc, tc)) {
        return;
    }
    if (tc.magic != proto::TestControl::kMagic) {
        LOG_WRN("TEST_CONTROL with wrong magic 0x%04x ignored", tc.magic);
        return;
    }
    if (tc.target_node != proto::Node::kAll && tc.target_node != role::kNode) {
        return;
    }
    {
        Locked l(shared().lock);
        ++shared().test_commands;
        shared().error_flags |= kTestHookActive;
    }
    const unsigned arg0 = tc.arg0 == 0U ? 1U : tc.arg0;  // "at least one" for the counting commands

    switch (tc.command) {
    case proto::TestCommand::kNop:
        break;
    case proto::TestCommand::kResetFaults:
        ecu_reset_faults();
        LOG_INF("TEST_CONTROL RESET_FAULTS");
        break;
    case proto::TestCommand::kInjectCanError:
        // arg0 transmit error events on the simulated controller (TEC += 8 each):
        // 12 reach the warning limit, 16 error-passive, 32 bus-off.
        can().sim_inject(ecu::ErrorCounterModel::Event::kTxError, arg0);
        LOG_INF("TEST_CONTROL INJECT_CAN_ERROR x%u -> %s tec=%u rec=%u", arg0, proto::to_string(can().state()),
                can().tec(), can().rec());
        break;
    case proto::TestCommand::kForceBusOff:
        can().sim_force_bus_off();
        LOG_INF("TEST_CONTROL FORCE_BUS_OFF -> %s", proto::to_string(can().state()));
        break;
    case proto::TestCommand::kSuppressHeartbeat:
        {
            Locked l(shared().lock);
            shared().tx_faults.suppress_heartbeat_until_ms = now_ms + tc.arg0;
        }
        LOG_INF("TEST_CONTROL SUPPRESS_HEARTBEAT for %u ms", tc.arg0);
        break;
    case proto::TestCommand::kCorruptCrc:
        {
            Locked l(shared().lock);
            shared().tx_faults.corrupt_crc_remaining += arg0;
        }
        LOG_INF("TEST_CONTROL CORRUPT_CRC x%u", arg0);
        break;
    case proto::TestCommand::kSkipCounter:
        {
            Locked l(shared().lock);
            shared().tx_faults.skip_counter_remaining += arg0;
        }
        LOG_INF("TEST_CONTROL SKIP_COUNTER x%u", arg0);
        break;
    case proto::TestCommand::kFreezeCounter:
        {
            Locked l(shared().lock);
            shared().tx_faults.freeze_counter_remaining += arg0;
        }
        LOG_INF("TEST_CONTROL FREEZE_COUNTER x%u", arg0);
        break;
    case proto::TestCommand::kSuppressMessage:
        {
            Locked l(shared().lock);
            shared().tx_faults.suppress_id = tc.arg0;
            shared().tx_faults.suppress_id_until_ms = now_ms + tc.arg1;
        }
        LOG_INF("TEST_CONTROL SUPPRESS_MESSAGE 0x%03x for %u ms", tc.arg0, tc.arg1);
        break;
    case proto::TestCommand::kStallThread:
        if (tc.arg0 >= kThreadCount) {
            LOG_WRN("TEST_CONTROL STALL_THREAD: no thread %u", tc.arg0);
            break;
        }
        {
            Locked l(shared().lock);
            shared().stall_ms[tc.arg0] = tc.arg1;
        }
        LOG_INF("TEST_CONTROL STALL_THREAD %u for %u ms", tc.arg0, tc.arg1);
        break;
    }
}

#else

void handle(const can_frame& /*frame*/, std::int64_t /*now_ms*/) {}

#endif

}  // namespace app::test_control

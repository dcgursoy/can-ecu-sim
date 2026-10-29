"""Group 5: error-passive and bus-off transitions and recovery time.

A virtual bus never produces electrical errors, so the controller's HAL
carries a simulated CAN controller (ISO 11898-1 error counters) that
TEST_CONTROL feeds: INJECT_CAN_ERROR adds transmit errors (TEC += 8 each),
FORCE_BUS_OFF pushes TEC past 255. From there everything is the real firmware:
the error state machine (ecu/can_error_sm.hpp) stops transmitting, backs off,
asks the HAL to recover (a modelled 3 ms recovery sequence). Measured on the
bus: the silence of the controller's commands while it is bus-off, the time in
error-passive, the safe-state entry and the return to NORMAL; from the
firmware log: the same transitions in the ECU's own clock; from `ecu status`:
the attempt and recovery counters. The doubling backoff and the FAULT latch of
a node that keeps going bus-off are in tests/test_bus_off_escalation.py.
"""

from __future__ import annotations

import time

import fault_scenarios as fs
from harness import NODE_CONTROLLER, SIM_RECOVERY_DELAY_MS, attempts, firmware_log, times_of

CMD_PERIOD = 20.0


def test_error_passive_entry_and_healing(net, measure):
    errors = 20  # TEC 160: error-passive until successful transmissions heal it to 127
    tec = errors * 8
    tx_rate = 1000.0 / net.period_ms("CONTROLLER_COMMAND") + 1000.0 / net.period_ms("CONTROLLER_HEARTBEAT")
    expected_ms = (tec - 127) / tx_rate * 1000.0
    hb = net.msg["CONTROLLER_HEARTBEAT"]
    cmd = net.id["CONTROLLER_COMMAND"]
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.send_test_control(NODE_CONTROLLER, "INJECT_CAN_ERROR", errors), 1.5)
        states = fs.decode_values(exp.fb, hb, "can_state", exp.before.t, exp.after.t)
        t_passive_hb = fs.first_at(states, "ERROR_PASSIVE", exp.t_apply)
        t_active_hb = fs.first_at(states, "ERROR_ACTIVE", t_passive_hb)
        log = firmware_log(net.controller.log_path, exp.before.controller_log, "can:")
        t_in = next((t for t, line in log if "-> ERROR_PASSIVE" in line), None)
        t_out = next((t for t, line in log if "ERROR_PASSIVE -> ERROR_ACTIVE" in line), None)
        passive_ms = (t_out - t_in) * 1000.0 if t_in is not None and t_out is not None else float("nan")
        ts = times_of(exp.fb, cmd, exp.t_apply, exp.t_apply + 0.5)
        rate = (len(ts) - 1) / (ts[-1] - ts[0]) if len(ts) > 1 else 0.0
        measure.check("ERROR_PASSIVE visible in the controller heartbeat after the injection",
                      (t_passive_hb - exp.t_apply) * 1000.0 if t_passive_hb else float("nan"), "ms", 0,
                      net.period_ms("CONTROLLER_HEARTBEAT") + net.tick_ms + 40, note=f"INJECT_CAN_ERROR x{errors}: TEC {tec}")
        measure.check("time in error-passive (firmware log, ECU clock)", passive_ms, "ms", expected_ms - 60, expected_ms + 60,
                      note=f"TEC heals by one per successful frame at {tx_rate:.0f}/s: {tec - 127} frames, {expected_ms:.0f} ms expected")
        measure.value("time in error-passive (heartbeats, 100 ms granularity)",
                      (t_active_hb - t_passive_hb) * 1000.0 if t_active_hb and t_passive_hb else float("nan"), "ms")
        measure.check("CONTROLLER_COMMAND rate while error-passive (transmission continues)", rate, "frames/s",
                      1000.0 / CMD_PERIOD * 0.9, 1000.0 / CMD_PERIOD * 1.1)
        measure.expect("one passive event, no bus-off, no safe state (error-passive is not an input loss)",
                       fs.delta(exp.before, exp.after, "controller", "can", "passive") == 1
                       and fs.delta(exp.before, exp.after, "controller", "can", "bus_off") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0)
        can = exp.after.controller.get("can", {})
        measure.expect("controller ERROR_ACTIVE and RUNNING again, TEC healed",
                       can.get("state") == "ERROR_ACTIVE" and fs.state_of(exp.after, "controller") == "RUNNING",
                       f"state {can.get('state')} tec={can.get('tec')}")
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_bus_off_recovery_after_the_backoff(net, measure):
    backoff = net.cfg["BUSOFF_BACKOFF_INITIAL_MS"]
    hold = net.cfg["SAFE_STATE_RECOVERY_MS"]
    tick = net.tick_ms
    cmd = net.id["CONTROLLER_COMMAND"]
    cmd_msg = net.msg["CONTROLLER_COMMAND"]
    hb = net.msg["CONTROLLER_HEARTBEAT"]
    for attempt in attempts(net, measure):
        if attempt.n > 1:
            # A bus-off within the stable window of a recovery is a failed attempt
            # with a doubled backoff (tests/test_bus_off_escalation.py); a retry
            # must wait that window out to measure the first backoff again.
            time.sleep(net.cfg["BUSOFF_STABLE_MS"] / 1000.0 + 0.5)
        exp = net.phase(lambda: net.send_test_control(NODE_CONTROLLER, "FORCE_BUS_OFF"), 1.2)
        gaps = fs.gaps(exp.fb, cmd, 50, exp.t_apply - 0.03, exp.after.t)
        silence = gaps[0][1] if len(gaps) == 1 else float("nan")
        t_gap_end = gaps[0][0] + gaps[0][1] / 1000.0 if gaps else None
        log = firmware_log(net.controller.log_path, exp.before.controller_log, "can:", "can (polled)", "bus-off recovery")
        t_off = next((t for t, line in log if "-> BUS_OFF" in line), None)
        t_on = next((t for t, line in log if "BUS_OFF -> ERROR_ACTIVE" in line), None)
        fw_recovery = (t_on - t_off) * 1000.0 if t_off is not None and t_on is not None else float("nan")
        modes = fs.decode_values(exp.fb, cmd_msg, "mode", exp.before.t, exp.after.t)
        first_mode = next((v for t, v in modes if t_gap_end and t >= t_gap_end - 1e-6), None)
        t_normal = fs.first_at(modes, "NORMAL", t_gap_end)
        flags = fs.decode_values(exp.fb, hb, "error_flags", exp.t_apply, exp.after.t)
        counts = fs.decode_values(exp.fb, hb, "bus_off_count", exp.t_apply, exp.after.t)
        can = exp.after.controller.get("can", {})
        sensor_log = fs.log_since(net.sensor.log_path, exp.before.sensor_log, "node_state:")

        measure.expect("exactly one gap in the controller's commands", len(gaps) == 1, len(gaps))
        measure.check("CONTROLLER_COMMAND silence on the bus (bus-off, backoff, recovery)", silence, "ms", backoff,
                      backoff + SIM_RECOVERY_DELAY_MS + tick + CMD_PERIOD + 40,
                      note=f"backoff {backoff} ms + modelled recovery {SIM_RECOVERY_DELAY_MS} ms + supervisor tick + next TX slot")
        measure.check("bus-off to error-active in the firmware log (ECU clock)", fw_recovery, "ms", backoff,
                      backoff + SIM_RECOVERY_DELAY_MS + 2 * tick + 5)
        measure.value("last_recovery_ms reported by `ecu status`", can.get("last_recovery_ms", "?"), "ms")
        measure.expect("one bus-off, one attempt, one recovery ok, no failure, phase active",
                       fs.delta(exp.before, exp.after, "controller", "can", "bus_off") == 1
                       and fs.delta(exp.before, exp.after, "controller", "can", "recovery_attempts") == 1
                       and fs.delta(exp.before, exp.after, "controller", "can", "recoveries_ok") == 1
                       and fs.delta(exp.before, exp.after, "controller", "can", "recovery_failures") == 0
                       and can.get("phase") == "active", f"phase {can.get('phase')}")
        measure.expect("bus-off is an input loss: one safe-state entry, the first command after the gap is SAFE_STATE",
                       fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 1 and first_mode == "SAFE_STATE",
                       f"first mode {first_mode}")
        measure.check("NORMAL again after the first command following recovery",
                      (t_normal - t_gap_end) * 1000.0 if t_normal and t_gap_end else float("nan"), "ms", hold - 4,
                      hold + tick + CMD_PERIOD + 40, note=f"recovery hold {hold} ms")
        measure.expect("heartbeat after recovery: BUS_OFF_SEEN flag set, bus_off_count 1",
                       any(int(v) & 0x08 for _, v in flags) and any(int(v) == 1 for _, v in counts))
        measure.expect("sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again",
                       fs.rx_delta(exp.before, exp.after, "sensor", "timeouts", "CONTROLLER_COMMAND") == 1
                       and any("-> DEGRADED" in line for line in sensor_log) and fs.state_of(exp.after, "sensor") == "RUNNING",
                       "; ".join(sensor_log))
        attempt.stalls = net.stalls(exp, exclude_own=[(exp.t_apply - 0.03, exp.t_apply + 0.5)])
    measure.verify()

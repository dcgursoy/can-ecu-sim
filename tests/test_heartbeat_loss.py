"""Group 6: heartbeat loss drives the controller into its safe state.

Three ways to lose the sensor: the sensor process is killed and restarted (a
dead node, then a cold restart with the rolling counters at zero), the sensor's
TX thread is stalled until the watchdog resets it (a hung thread; the liveness
monitor withholds the watchdog feed), and the sensor suppresses its heartbeat
(a firmware hook: the node is alive, its liveness message is not). Measured on
the bus: the time from the last frame to the SAFE_STATE command, the safe
values of the commands meanwhile, and the return to NORMAL after the
configured recovery hold. The order matters for the firmware coverage of the
instrumented builds: a killed process and a re-executed one write no coverage
data, so the hook that the surviving process handles comes last.
"""

from __future__ import annotations

import time

import fault_scenarios as fs
from harness import (NODE_SENSOR, SENSOR_MESSAGES, THREAD_TX, Experiment, attempts, first_after, firmware_log,
                     last_before)

CMD_PERIOD = 20.0


def test_sensor_process_death_and_restart(net, measure):
    timeout = net.timeout_ms("SENSOR_DATA")
    hold = net.cfg["SAFE_STATE_RECOVERY_MS"]
    tick = net.tick_ms
    sd, hb = net.id["SENSOR_DATA"], net.id["SENSOR_HEARTBEAT"]
    cmd_msg = net.msg["CONTROLLER_COMMAND"]
    ids = [net.id[m] for m in SENSOR_MESSAGES]
    for attempt in attempts(net, measure):
        net.reset_faults()
        net.wait_running()
        before = net.snapshot()
        net.gw.stats(reset=True)
        t_kill = net.sensor.kill()
        net.note(f"sensor killed (SIGKILL) at {t_kill:.3f}")
        time.sleep(1.5)
        while_dead = net.controller.status()
        t_restart = net.restart("sensor")
        net.wait_running(timeout_s=8.0)
        time.sleep(1.0)
        after = net.snapshot()
        fa, fb = net.frames(True), net.frames(False)
        last_data = last_before(fb, sd, t_kill)
        modes = fs.decode_values(fb, cmd_msg, "mode", before.t, after.t)
        t_safe = fs.first_at(modes, "SAFE_STATE", last_data)
        first_frame = min((t for t in (first_after(fa, i, t_restart) for i in ids) if t is not None), default=None)
        first_hb = first_after(fa, hb, t_restart)
        t_normal = fs.first_at(modes, "NORMAL", first_hb)
        measure.check("SAFE_STATE command after the last SENSOR_DATA of the killed sensor",
                      (t_safe - last_data) * 1000.0 if t_safe and last_data else float("nan"), "ms", timeout - 4,
                      timeout + tick + CMD_PERIOD + 40,
                      note=f"the {timeout} ms SENSOR_DATA timeout fires first; the heartbeat timeout would take {net.timeout_ms('SENSOR_HEARTBEAT')} ms")
        measure.value("time from the kill to the SAFE_STATE command", (t_safe - t_kill) * 1000.0 if t_safe else float("nan"), "ms")
        measure.expect("controller in SAFE_STATE while the sensor is dead",
                       str(while_dead.get("node", {}).get("state")) == "SAFE_STATE", while_dead.get("node", {}).get("state"))
        measure.check("restarted sensor: first frame after the process start",
                      (first_frame - t_restart) * 1000.0 if first_frame else float("nan"), "ms", hi=1000.0,
                      note="process start, Zephyr boot, first TX tick")
        measure.check("restarted sensor: first heartbeat after its first frame",
                      (first_hb - first_frame) * 1000.0 if first_hb and first_frame else float("nan"), "ms", 0,
                      net.period_ms("SENSOR_HEARTBEAT") + 10)
        measure.check("NORMAL command after the first heartbeat of the restarted sensor",
                      (t_normal - first_hb) * 1000.0 if t_normal and first_hb else float("nan"), "ms", hold - 4,
                      hold + tick + CMD_PERIOD + 40, note=f"both critical inputs alive from the heartbeat on; hold {hold} ms")
        timeouts = {m: fs.rx_delta(before, after, "controller", "timeouts", m) for m in SENSOR_MESSAGES}
        measure.expect("controller: one timeout per sensor message, one safe-state entry",
                       all(v == 1 for v in timeouts.values())
                       and fs.delta(before, after, "controller", "node", "safe_state_entries") == 1, str(timeouts))
        measure.expect("the restarted sensor's counters (from 0) are accepted: no counter rejections, no CRC faults",
                       fs.rx_delta(before, after, "controller", "counter") == 0 and fs.rx_delta(before, after, "controller", "crc") == 0,
                       note="the timeout reset the receiver's counter history")
        measure.expect("both RUNNING at the end",
                       fs.state_of(after, "controller") == "RUNNING" and fs.state_of(after, "sensor") == "RUNNING")
        window = [(t_kill - 0.05, (first_frame or t_restart) + 0.05)]
        exp = Experiment(before, after, t_kill, t_restart, fa, fb, net.gw.stats())
        attempt.stalls = net.stalls(exp, exclude_own=window, exclude_relay=window)
    measure.verify()


def test_stalled_thread_is_reset_by_the_watchdog(net, measure):
    stall_ms = 1500
    watchdog = net.cfg["WATCHDOG_TIMEOUT_MS"]
    liveness = net.cfg["LIVENESS_WINDOW_MS"]
    tick = net.tick_ms
    sd = net.id["SENSOR_DATA"]
    for attempt in attempts(net, measure):
        net.reset_faults()
        net.wait_running()
        before = net.snapshot()
        net.gw.stats(reset=True)
        t0 = time.time()
        net.send_test_control(NODE_SENSOR, "STALL_THREAD", THREAD_TX, stall_ms)
        time.sleep(3.0)
        restarted = False
        if not net.sensor.running():
            net.restart("sensor")  # the simulator exited on the reset instead of re-executing itself
            restarted = True
            time.sleep(1.5)
        t_status = time.time()
        after = net.snapshot()
        fa, fb = net.frames(True), net.frames(False)
        silence = fs.gaps(fa, sd, 100, t0 - 0.05, after.t)
        log = firmware_log(net.sensor.log_path, before.sensor_log, "test hook", "watchdog not fed")
        t_block = next((t for t, line in log if "thread blocked" in line), None)
        t_not_fed = next((t for t, line in log if "watchdog not fed" in line), None)
        banner = fs.log_since(net.sensor.log_path, before.sensor_log, "Booting Zephyr")
        uptime = fs.ival(after.sensor, "ecu", "uptime_ms")
        # The rebooted process's uptime dates its reboot in host time (the status
        # was read within a few ms of t_status), which splits the silence into the
        # firmware's part (the watchdog timeout) and the host's (exec, boot).
        t_reboot = t_status - uptime / 1000.0 if uptime > 0 else None
        t_last = silence[0][0] if len(silence) == 1 else None
        t_first = t_last + silence[0][1] / 1000.0 if t_last is not None else None
        measure.expect("exactly one gap in SENSOR_DATA", len(silence) == 1, len(silence))
        measure.value("SENSOR_DATA silence on the bus (stall, watchdog reset, reboot, first TX tick)",
                      silence[0][1] if len(silence) == 1 else float("nan"), "ms")
        measure.check("watchdog reset after the last frame (firmware uptime dates the reboot)",
                      (t_reboot - t_last) * 1000.0 if t_reboot and t_last else float("nan"), "ms",
                      liveness + watchdog - 10, liveness + watchdog + 2 * tick + 20,
                      note=f"the supervisor feeds the watchdog until the stalled thread is {liveness} ms stale, "
                           f"then the watchdog's {watchdog} ms run out")
        measure.check("reboot to the first frame of the rebooted sensor (host: exec, Zephyr boot, first TX tick)",
                      (t_first - t_reboot) * 1000.0 if t_first and t_reboot else float("nan"), "ms", hi=1500.0,
                      note="host time, not firmware time: the executable is re-executed from the bind mount")
        measure.check("watchdog feed withheld after the liveness window (firmware log, ECU clock)",
                      (t_not_fed - t_block) * 1000.0 if t_block is not None and t_not_fed is not None else float("nan"), "ms",
                      liveness - 10, liveness + 2 * tick + 20, note=f"liveness window {liveness} ms")
        measure.expect("sensor rebooted in place: boot banner again, uptime restarted, same process",
                       bool(banner) and uptime < (after.t - t0) * 1000.0 and not restarted,
                       f"uptime {uptime} ms, {'restarted by the test' if restarted else 'same pid'}")
        measure.expect("controller: one safe-state entry, RUNNING again",
                       fs.delta(before, after, "controller", "node", "safe_state_entries") == 1
                       and fs.state_of(after, "controller") == "RUNNING")
        measure.expect("sensor RUNNING again", fs.state_of(after, "sensor") == "RUNNING")
        window = [(t0 - 0.05, t0 + 3.0)]
        exp = Experiment(before, after, t0, t0 + 3.0, fa, fb, net.gw.stats())
        attempt.stalls = net.stalls(exp, exclude_own=window, exclude_relay=window)
    measure.verify()
def test_heartbeat_loss_drives_the_controller_into_safe_state(net, measure):
    suppress_ms = 600
    timeout = net.timeout_ms("SENSOR_HEARTBEAT")
    hold = net.cfg["SAFE_STATE_RECOVERY_MS"]
    tick = net.tick_ms
    hb = net.id["SENSOR_HEARTBEAT"]
    cmd = net.id["CONTROLLER_COMMAND"]
    cmd_msg = net.msg["CONTROLLER_COMMAND"]
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.send_test_control(NODE_SENSOR, "SUPPRESS_HEARTBEAT", suppress_ms), 2.0)
        inside = exp.t_apply + suppress_ms / 1000.0 * 0.6
        last_hb = last_before(exp.fa, hb, inside)
        first_hb_after = first_after(exp.fa, hb, inside)
        modes = fs.decode_values(exp.fb, cmd_msg, "mode", exp.before.t, exp.after.t)
        t_safe = fs.first_at(modes, "SAFE_STATE", last_hb)
        t_normal = fs.first_at(modes, "NORMAL", first_hb_after)
        suppressed = fs.delta(exp.before, exp.after, "sensor", "hooks", "suppressed")
        safe_cmds = [cmd_msg.decode(bytes(d), decode_choices=False)
                     for t, i, d in exp.fb if i == cmd and t_safe and t_normal and t_safe <= t < t_normal]
        safe_values = bool(safe_cmds) and all(
            c["actuators_enabled"] == 0 and c["brake_hold"] == 1 and c["speed_limit"] == 0 and c["torque_cmd"] == 0
            for c in safe_cmds)
        measure.value("heartbeats suppressed by the sensor", suppressed, "frames", note=f"SUPPRESS_HEARTBEAT {suppress_ms} ms")
        hb_period = net.period_ms("SENSOR_HEARTBEAT")
        measure.check("SENSOR_HEARTBEAT silence on the bus", (first_hb_after - last_hb) * 1000.0 if first_hb_after and last_hb else float("nan"),
                      "ms", suppress_ms - 10, suppress_ms + 2 * hb_period + 10,
                      note=f"{suppress_ms} ms of suppression starts and ends between two {hb_period:.0f} ms heartbeat slots")
        measure.check("SAFE_STATE command after the last heartbeat", (t_safe - last_hb) * 1000.0 if t_safe and last_hb else float("nan"),
                      "ms", timeout - 4, timeout + tick + CMD_PERIOD + 40,
                      note=f"heartbeat timeout {timeout} ms + supervisor tick + command period")
        measure.check("NORMAL command after the first heartbeat again",
                      (t_normal - first_hb_after) * 1000.0 if t_normal and first_hb_after else float("nan"), "ms", hold - 4,
                      hold + tick + CMD_PERIOD + 40, note=f"recovery hold {hold} ms + tick + command period")
        measure.expect("every command in the safe state: actuators off, brake hold, speed limit 0, torque 0", safe_values,
                       f"{len(safe_cmds)} commands over {(t_normal - t_safe) * 1000:.0f} ms" if t_safe and t_normal else "n/a")
        measure.expect("controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, SENSOR_DATA never timed out, RUNNING again",
                       fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 1
                       and fs.rx_delta(exp.before, exp.after, "controller", "timeouts", "SENSOR_HEARTBEAT") == 1
                       and fs.rx_delta(exp.before, exp.after, "controller", "timeouts", "SENSOR_DATA") == 0
                       and fs.state_of(exp.after, "controller") == "RUNNING")
        attempt.stalls = net.stalls(exp)
    measure.verify()

"""Group 5, continued: a node whose recoveries do not hold.

Every bus-off within the stable window of the previous recovery counts as a
failed attempt (ecu/can_error_sm.hpp): the backoff doubles up to its cap and,
after the configured number of attempts, the node latches FAULT and stops
transmitting until an operator clears it (`ecu reset`). The chain is measured
on the bus as the lengths of the controller's silences; it lives in its own
module so that it starts from a fresh process, with no recovery in its recent
history.
"""

from __future__ import annotations

import time

import fault_scenarios as fs
from harness import NODE_CONTROLLER, SIM_RECOVERY_DELAY_MS, Experiment, attempts, first_after, times_of

CMD_PERIOD = 20.0


def wait_can_phase(net, phase: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if net.status("controller").get("can", {}).get("phase") == phase:
            return True
        time.sleep(0.05)
    return False


def test_repeated_bus_off_doubles_the_backoff_until_fault(net, measure):
    initial = net.cfg["BUSOFF_BACKOFF_INITIAL_MS"]
    cap = net.cfg["BUSOFF_BACKOFF_MAX_MS"]
    max_attempts = net.cfg["BUSOFF_MAX_ATTEMPTS"]
    stable = net.cfg["BUSOFF_STABLE_MS"]
    hold = net.cfg["SAFE_STATE_RECOVERY_MS"]
    tick = net.tick_ms
    cmd = net.id["CONTROLLER_COMMAND"]
    cmd_msg = net.msg["CONTROLLER_COMMAND"]
    backoffs = [min(initial * 2 ** k, cap) for k in range(max_attempts)]  # the recoveries that are attempted
    for attempt in attempts(net, measure):
        if attempt.n > 1:
            time.sleep(stable / 1000.0 + 0.5)  # a retry must not inherit the previous attempt's recovery history
        net.reset_faults()
        net.wait_running()
        before = net.snapshot()
        net.gw.stats(reset=True)
        forced: list[float] = []
        recovered: list[bool] = []
        for k in range(max_attempts + 1):
            forced.append(time.time())
            net.send_test_control(NODE_CONTROLLER, "FORCE_BUS_OFF")
            if k < max_attempts:
                # The next bus-off must come after this recovery completed (a bus-off
                # during the backoff is the same event) and within the stable window.
                recovered.append(wait_can_phase(net, "active", backoffs[k] / 1000.0 + 1.0))
                time.sleep(0.15)
            else:
                time.sleep(1.0)
        mid = net.snapshot()
        fb = net.frames(False)
        gaps = fs.gaps(fb, cmd, 50, forced[0] - 0.03, mid.t)
        for k, expected in enumerate(backoffs):
            gap = gaps[k][1] if k < len(gaps) else float("nan")
            measure.check(f"bus-off #{k + 1}: command silence", gap, "ms", expected,
                          expected + SIM_RECOVERY_DELAY_MS + tick + CMD_PERIOD + 40,
                          note=f"backoff {expected} ms" + (" (doubled: the previous recovery did not hold)" if k else ""))
        measure.expect(f"every attempted recovery completed before the next bus-off ({max_attempts})", all(recovered), recovered)
        can = mid.controller.get("can", {})
        after_last = len(times_of(fb, cmd, forced[-1] + 0.05, mid.t))
        measure.expect(f"bus-off #{max_attempts + 1}: no recovery, phase failed, node FAULT, transmission stopped",
                       can.get("phase") == "failed" and fs.state_of(mid, "controller") == "FAULT" and after_last == 0,
                       f"phase {can.get('phase')}, state {fs.state_of(mid, 'controller')}, {after_last} commands after it")
        measure.expect(f"{max_attempts} attempts, {max_attempts} recoveries ok, {max_attempts} failures (the attempts are exhausted)",
                       fs.delta(before, mid, "controller", "can", "recovery_attempts") == max_attempts
                       and fs.delta(before, mid, "controller", "can", "recoveries_ok") == max_attempts
                       and fs.delta(before, mid, "controller", "can", "recovery_failures") == max_attempts,
                       f"attempts {fs.delta(before, mid, 'controller', 'can', 'recovery_attempts')}, "
                       f"ok {fs.delta(before, mid, 'controller', 'can', 'recoveries_ok')}, "
                       f"failures {fs.delta(before, mid, 'controller', 'can', 'recovery_failures')}")

        t_reset = time.time()
        net.controller.shell("ecu reset", done="faults cleared")
        time.sleep(initial / 1000.0 + hold / 1000.0 + 0.6)
        after = net.snapshot()
        fa, fb = net.frames(True), net.frames(False)
        t_first = first_after(fb, cmd, t_reset)
        modes = fs.decode_values(fb, cmd_msg, "mode", t_reset, after.t)
        t_normal = fs.first_at(modes, "NORMAL", t_first)
        measure.check("first command after `ecu reset`", (t_first - t_reset) * 1000.0 if t_first else float("nan"), "ms",
                      initial, initial + SIM_RECOVERY_DELAY_MS + tick + CMD_PERIOD + 60,
                      note="the reset restarts the backoff from the first attempt; the controller is still bus-off")
        measure.check("NORMAL again after `ecu reset`", (t_normal - t_reset) * 1000.0 if t_normal else float("nan"), "ms",
                      initial + hold, initial + hold + SIM_RECOVERY_DELAY_MS + 2 * tick + CMD_PERIOD + 60,
                      note=f"backoff {initial} ms + recovery hold {hold} ms")
        can = after.controller.get("can", {})
        measure.expect("controller RUNNING, CAN phase active after the reset",
                       fs.state_of(after, "controller") == "RUNNING" and can.get("phase") == "active",
                       f"state {fs.state_of(after, 'controller')}, phase {can.get('phase')}")
        timeouts = fs.rx_delta(before, after, "sensor", "timeouts", "CONTROLLER_COMMAND")
        measure.expect(f"sensor: one CONTROLLER_COMMAND timeout per silence ({max_attempts + 1}), RUNNING again",
                       timeouts == max_attempts + 1 and fs.state_of(after, "sensor") == "RUNNING", timeouts)
        exp = Experiment(before, after, forced[0], t_reset, fa, fb, net.gw.stats())
        attempt.stalls = net.stalls(exp, exclude_own=[(forced[0] - 0.05, t_reset + 0.3)])
    measure.verify()

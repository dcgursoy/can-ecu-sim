"""Group 2: how long the controller (and the sensor) take to detect a missing message.

The gateway drops every frame of one identifier in one direction for a while.
The receiver's timeout monitor fires, the supervisor acts on its next tick and
the reaction is published by the next frame of the message that carries it:
CONTROLLER_COMMAND.mode (every 20 ms) for the controller's safe state,
the heartbeat's node_state (every 100 ms) for DEGRADED. Detection is measured
on the bus from the last delivered frame to that reaction, so it includes the
whole chain and is what another node on a real bus would see; the firmware's
own view (one timeout counted, one safe-state entry) is checked alongside.
Recovery is measured from the first frame delivered again to the reaction
being withdrawn (the controller holds its safe state for a configured time).
"""

from __future__ import annotations

import pytest

import fault_scenarios as fs
from harness import CONTROLLER_MESSAGES, SENSOR_MESSAGES, attempts, first_after, last_before

MONITORED = {"controller": SENSOR_MESSAGES, "sensor": CONTROLLER_MESSAGES}
OTHER = {"controller": "sensor", "sensor": "controller"}

CASES = [
    # dropped message, gateway direction, detecting node, message.signal that shows the reaction, reaction, recovery
    ("SENSOR_DATA", "a_to_b", "controller", "CONTROLLER_COMMAND", "mode", "SAFE_STATE", "NORMAL"),
    ("SENSOR_HEARTBEAT", "a_to_b", "controller", "CONTROLLER_COMMAND", "mode", "SAFE_STATE", "NORMAL"),
    ("BATTERY_STATUS", "a_to_b", "controller", "CONTROLLER_HEARTBEAT", "node_state", "DEGRADED", "RUNNING"),
    ("CONTROLLER_COMMAND", "b_to_a", "sensor", "SENSOR_HEARTBEAT", "node_state", "DEGRADED", "RUNNING"),
]


@pytest.mark.parametrize("name,direction,role,react_msg,signal,reaction,recovered", CASES, ids=[c[0] for c in CASES])
def test_missing_message_is_detected_within_its_timeout(net, measure, name, direction, role, react_msg, signal,
                                                        reaction, recovered):
    timeout = net.timeout_ms(name)
    tick = net.tick_ms
    publish = net.period_ms(react_msg)
    safe = reaction == "SAFE_STATE"
    hold_ms = net.cfg["SAFE_STATE_RECOVERY_MS"]
    hold_s = timeout / 1000.0 + 0.4
    settle_s = (hold_ms / 1000.0 if safe else 0.0) + 0.4
    cfg = {"drop": 1.0, "ids": [hex(net.id[name])]}
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults(**{direction: cfg}), hold_s, net.gw.faults, settle_s)
        delivered = exp.frames(direction == "b_to_a")  # the receiver's segment
        shown = exp.frames(net.own_iface_is_a(react_msg))
        t_last = last_before(delivered, net.id[name], exp.t_clear)
        t_first = first_after(delivered, net.id[name], exp.t_clear)
        values = fs.decode_values(shown, net.msg[react_msg], signal, exp.before.t, exp.after.t)
        t_react = fs.first_at(values, reaction, t_last)
        t_back = fs.first_at(values, recovered, t_first)
        detection = (t_react - t_last) * 1000.0 if t_react and t_last else float("nan")
        recovery = (t_back - t_first) * 1000.0 if t_back and t_first else float("nan")

        measure.value(f"{name}: frames dropped by the gateway", exp.gw[direction]["dropped"], "frames",
                      note=f"{hold_s * 1000:.0f} ms of loss, period {net.period_ms(name):.0f} ms")
        measure.check(f"{name}: {role} reaction ({react_msg}.{signal}={reaction}) after the last delivered frame",
                      detection, "ms", timeout - 4, timeout + tick + publish + 40,
                      note=f"timeout {timeout} ms + supervisor tick {tick} ms + {publish:.0f} ms publish period")
        if safe:
            measure.check(f"{name}: {role} back to {recovered} after the first frame delivered again", recovery, "ms",
                          hold_ms - 4, hold_ms + tick + publish + 40,
                          note=f"recovery hold {hold_ms} ms + tick + publish period")
        else:
            measure.check(f"{name}: {role} back to {recovered} after the first frame delivered again", recovery, "ms",
                          0, tick + publish + 40, note="no hold before leaving DEGRADED")
        timeouts = fs.rx_delta(exp.before, exp.after, role, "timeouts", name)
        others = sum(fs.rx_delta(exp.before, exp.after, role, "timeouts", m) for m in MONITORED[role] if m != name)
        measure.expect(f"{name}: exactly one timeout counted by the {role}, none for its other inputs",
                       timeouts == 1 and others == 0, f"{timeouts} for {name}, {others} others")
        entries = fs.delta(exp.before, exp.after, role, "node", "safe_state_entries")
        measure.expect(f"{name}: {role} safe-state entries == {1 if safe else 0}, RUNNING again",
                       entries == (1 if safe else 0) and fs.state_of(exp.after, role) == "RUNNING",
                       f"{entries} entries, state {fs.state_of(exp.after, role)}")
        other = OTHER[role]
        measure.expect(f"{name}: {other} unaffected (no timeouts, no safe state, RUNNING)",
                       fs.rx_delta(exp.before, exp.after, other, "timeouts") == 0
                       and fs.delta(exp.before, exp.after, other, "node", "safe_state_entries") == 0
                       and fs.state_of(exp.after, other) == "RUNNING")
        window = [(exp.t_apply - 0.05, exp.t_clear + 0.1)]
        attempt.stalls = net.stalls(exp, exclude_relay=window)
    measure.verify()

"""Group 3: bad CRCs and stale or skipped rolling counters.

Faults come from both layers: the gateway corrupts or duplicates frames in
flight (a bus-level fault), the sensor's firmware hooks make it send frames
with a wrong CRC, a frozen counter or a skipped counter value (a faulty
sender). The receiver's policy (ecu/protected_rx.hpp): wrong CRC rejected,
repeated counter rejected, skipped counter accepted with the loss counted,
and every fault published in the heartbeat's error_flags. Measured: the counts
against what was injected, the time until the flag shows on the bus, and the
transition to DEGRADED after the configured number of consecutive rejects.
"""

from __future__ import annotations

import fault_scenarios as fs
from harness import FLAG, NODE_SENSOR, SENSOR_MESSAGES, attempts, first_after


def flag_after(frames, hb_msg, flag: int, t_start: float, t_from: float, t_to: float) -> tuple[float | None, bool]:
    """(time the heartbeat first shows `flag` after t_from, flag clear in every
    heartbeat between t_start (the snapshot after RESET_FAULTS) and t_from)."""
    values = fs.decode_values(frames, hb_msg, "error_flags", t_start, t_to)
    clear_before = not any(int(v) & flag for t, v in values if t <= t_from)
    t_set = next((t for t, v in values if t > t_from and int(v) & flag), None)
    return t_set, clear_before


def bad_crc_times(net, frames, t0: float, t1: float) -> list[float]:
    ids = {net.id[m] for m in SENSOR_MESSAGES}
    return [t for t, i, d in frames if t0 <= t <= t1 and i in ids and not fs.e2e_ref.crc_ok(i, bytes(d))]


def test_corrupted_frames_are_rejected_by_crc(net, measure):
    """one bit flipped in 10 % of the sensor->controller frames for 2 s"""
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults({"corrupt": 0.1}), 2.0, net.gw.faults, 1.5)
        gw = exp.gw["a_to_b"]
        crc = fs.rx_delta(exp.before, exp.after, "controller", "crc")
        measure.expect("controller CRC rejections == frames the gateway corrupted", crc == gw["corrupted"] > 0,
                       f"{crc} rejected, {gw['corrupted']} corrupted of {gw['received']}")
        events = []
        for name in SENSOR_MESSAGES:
            runs = fs.bad_crc_runs(exp.fb, net.id[name], exp.t_apply, exp.t_clear + 0.1)
            longest = max((n for _, _, n in runs), default=0)
            timeouts = [(s, e) for s, e, n in runs if (n + 1) * net.period_ms(name) > net.timeout_ms(name)]
            if name != "BATTERY_STATUS":
                events += timeouts
            counted = fs.rx_delta(exp.before, exp.after, "controller", "timeouts", name)
            measure.value(f"{name}: runs of consecutive corrupted frames", len(runs), "runs",
                          note=f"longest {longest}; a run of rejects is silence to the {net.timeout_ms(name)} ms timeout")
            measure.expect(f"{name}: timeouts == runs longer than the timeout ({len(timeouts)})", counted == len(timeouts),
                           counted)
        expected_safe = fs.merged_count(events)
        entries = fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries")
        measure.expect(f"controller safe-state entries == timeout runs on critical inputs ({expected_safe})",
                       entries == expected_safe, entries)
        measure.expect("controller RUNNING again; sensor unaffected",
                       fs.state_of(exp.after, "controller") == "RUNNING" and fs.state_of(exp.after, "sensor") == "RUNNING"
                       and fs.rx_delta(exp.before, exp.after, "sensor", "crc") == 0)
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_crc_fault_is_latched_in_the_heartbeat_flags(net, measure):
    """firmware hook: the sensor corrupts the CRC of its next two frames"""
    hb_period = net.period_ms("CONTROLLER_HEARTBEAT")
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.send_test_control(NODE_SENSOR, "CORRUPT_CRC", 2), 0.6)
        corrupted = fs.delta(exp.before, exp.after, "sensor", "hooks", "corrupted")
        crc = fs.rx_delta(exp.before, exp.after, "controller", "crc")
        bad = bad_crc_times(net, exp.fa, exp.t_apply, exp.after.t)
        t_bad = min(bad) if bad else None
        t_flag, clear_before = flag_after(exp.fb, net.msg["CONTROLLER_HEARTBEAT"], FLAG["CRC_FAULT_SEEN"],
                                          exp.before.t, t_bad or exp.t_apply, exp.after.t)
        latency = (t_flag - t_bad) * 1000.0 if t_flag and t_bad else float("nan")
        measure.expect("sensor corrupted 2 frames, controller rejected 2 by CRC", corrupted == 2 and crc == 2 and len(bad) == 2,
                       f"{corrupted} corrupted, {len(bad)} bad on the bus, {crc} rejected")
        measure.expect("CRC_FAULT_SEEN clear in the heartbeat before the fault (RESET_FAULTS)", clear_before)
        measure.check("CRC_FAULT_SEEN published in the controller heartbeat after the first bad frame", latency, "ms",
                      0, hb_period + net.tick_ms + 40, note="flag set on rejection, published by the next heartbeat")
        measure.expect("two bad frames are tolerated: no timeout, no safe state",
                       fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0)
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_stale_counter_is_rejected_as_repeated(net, measure):
    """a stuck sender (FREEZE_COUNTER x2) and a duplicating bus (20 % of frames twice)

    Two stale frames in a row: a rejected frame does not renew the timeout, so
    three consecutive rejects of a 10 ms message would be a 35 ms timeout and a
    safe-state entry (the corrupt-CRC tests show that side); two are tolerated."""
    hb_period = net.period_ms("CONTROLLER_HEARTBEAT")
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.send_test_control(NODE_SENSOR, "FREEZE_COUNTER", 2), 0.6)
        frozen = fs.delta(exp.before, exp.after, "sensor", "hooks", "frozen")
        rejected = fs.rx_delta(exp.before, exp.after, "controller", "counter")
        # the stale frames on the sensor's segment: same identifier, same counter as the frame before
        last: dict[int, int] = {}
        stale = []
        for t, i, d in exp.fa:
            if i not in {net.id[m] for m in SENSOR_MESSAGES} or len(d) < 2:
                continue
            counter = d[1] & 0x0F
            if exp.t_apply <= t <= exp.after.t and last.get(i) == counter:
                stale.append(t)
            last[i] = counter
        t_flag, clear_before = flag_after(exp.fb, net.msg["CONTROLLER_HEARTBEAT"], FLAG["COUNTER_FAULT_SEEN"],
                                          exp.before.t, stale[0] if stale else exp.t_apply, exp.after.t)
        latency = (t_flag - stale[0]) * 1000.0 if t_flag and stale else float("nan")
        measure.expect("sensor repeated the previous counter on 2 frames, controller rejected 2 as repeated",
                       frozen == 2 and rejected == 2 and len(stale) == 2,
                       f"{frozen} frozen, {len(stale)} stale on the bus, {rejected} rejected")
        measure.expect("COUNTER_FAULT_SEEN clear before the fault", clear_before)
        measure.check("COUNTER_FAULT_SEEN published after the first stale frame", latency, "ms", 0,
                      hb_period + net.tick_ms + 40)
        measure.expect("two stale frames are tolerated: no CRC fault, no timeout, no safe state",
                       fs.rx_delta(exp.before, exp.after, "controller", "crc") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0,
                       f"crc {fs.rx_delta(exp.before, exp.after, 'controller', 'crc')}, "
                       f"timeouts {fs.rx_delta(exp.before, exp.after, 'controller', 'timeouts')}, "
                       f"safe-state entries {fs.delta(exp.before, exp.after, 'controller', 'node', 'safe_state_entries')}")

        exp2 = net.phase(lambda: net.gw.faults({"duplicate": 0.2}), 2.0, net.gw.faults, 1.0)
        gw = exp2.gw["a_to_b"]
        rejected2 = fs.rx_delta(exp2.before, exp2.after, "controller", "counter")
        measure.expect("controller repeated-counter rejections == frames the gateway duplicated",
                       rejected2 == gw["duplicated"] > 0, f"{rejected2} rejected, {gw['duplicated']} duplicated of {gw['received']}")
        measure.expect("duplicates cause no CRC faults, timeouts or safe state; both RUNNING",
                       fs.rx_delta(exp2.before, exp2.after, "controller", "crc") == 0
                       and fs.rx_delta(exp2.before, exp2.after, "controller", "timeouts") == 0
                       and fs.delta(exp2.before, exp2.after, "controller", "node", "safe_state_entries") == 0
                       and fs.state_of(exp2.after, "controller") == "RUNNING" and fs.state_of(exp2.after, "sensor") == "RUNNING")
        attempt.stalls = net.stalls(exp) + net.stalls(exp2)
    measure.verify()


def test_skipped_counter_is_accepted_and_the_loss_counted(net, measure):
    """a sender that skips counter values (SKIP_COUNTER x3) and a short loss below the timeout"""
    sd = net.id["SENSOR_DATA"]
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.send_test_control(NODE_SENSOR, "SKIP_COUNTER", 3), 0.6)
        hooked = fs.delta(exp.before, exp.after, "sensor", "hooks", "skipped")
        skipped = fs.rx_delta(exp.before, exp.after, "controller", "skipped")
        lost = fs.rx_delta(exp.before, exp.after, "controller", "lost")
        rejected = fs.rx_delta(exp.before, exp.after, "controller", "counter")
        measure.expect("sensor skipped 3 counter values: controller accepted 3 frames with loss, 3 frames counted lost",
                       hooked == 3 and skipped == 3 and lost == 3, f"{hooked} skipped, {skipped} accepted with loss, {lost} lost")
        measure.expect("nothing rejected (the data is fresh), no timeout, no safe state",
                       rejected == 0 and fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0)

        # A loss shorter than the timeout: the counter accounts for it, the timeout does not fire.
        exp2 = net.phase(lambda: net.gw.faults({"drop": 1.0, "ids": [hex(sd)]}), 0.015, net.gw.faults, 0.4)
        dropped = exp2.gw["a_to_b"]["dropped"]
        gaps, missing, ambiguous = fs.loss_expectations(exp2.fb, sd, "SENSOR_DATA", exp2.t_apply - 0.05, exp2.t_clear + 0.1)
        lost2 = fs.rx_delta(exp2.before, exp2.after, "controller", "lost", "SENSOR_DATA")
        timeouts2 = fs.rx_delta(exp2.before, exp2.after, "controller", "timeouts", "SENSOR_DATA")
        largest = max((g for _, g in fs.gaps(exp2.fb, sd, 15, exp2.t_apply - 0.05, exp2.t_clear + 0.1)), default=0.0)
        measure.value("SENSOR_DATA frames dropped for 15 ms", dropped, "frames", note=f"largest gap on the bus {largest:.1f} ms")
        if ambiguous:
            measure.value("gap within 4 ms of the timeout: accounting not judged", True)
        else:
            measure.expect(f"timeouts == gaps over {net.timeout_ms('SENSOR_DATA')} ms ({len(gaps)})", timeouts2 == len(gaps), timeouts2)
            measure.expect("dropped == counted lost by the counter + frames inside timeout gaps", dropped == lost2 + missing,
                           f"{dropped} dropped, {lost2} lost, {missing} in timeout gaps")
        attempt.stalls = net.stalls(exp) + net.stalls(exp2, exclude_relay=[(exp2.t_apply - 0.05, exp2.t_clear + 0.1)])
    measure.verify()


def test_persistent_bad_frames_degrade_the_node(net, measure):
    """every BATTERY_STATUS corrupted for 350 ms: the configured number of consecutive rejects degrades the controller"""
    threshold = net.cfg["QUALITY_FAULT_THRESHOLD"]
    bat = net.id["BATTERY_STATUS"]
    hb = net.msg["CONTROLLER_HEARTBEAT"]
    hb_period = net.period_ms("CONTROLLER_HEARTBEAT")
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults({"corrupt": 1.0, "ids": [hex(bat)]}), 0.35, net.gw.faults, 1.0)
        bad = [t for t, i, d in exp.fb if i == bat and exp.t_apply <= t <= exp.t_clear + 0.1 and not fs.e2e_ref.crc_ok(i, bytes(d))]
        good_after = first_after([f for f in exp.fb if f[1] == bat and fs.e2e_ref.crc_ok(bat, bytes(f[2]))], bat, exp.t_clear)
        states = fs.decode_values(exp.fb, hb, "node_state", exp.before.t, exp.after.t)
        t_nth = bad[threshold - 1] if len(bad) >= threshold else None
        t_deg = fs.first_at(states, "DEGRADED", bad[0] if bad else None)
        t_run = fs.first_at(states, "RUNNING", t_deg)
        crc = fs.rx_delta(exp.before, exp.after, "controller", "crc", "BATTERY_STATUS")
        measure.expect(f"CRC rejections == corrupted BATTERY_STATUS frames (at least {threshold})",
                       crc == exp.gw["a_to_b"]["corrupted"] >= threshold, f"{crc} rejected, {exp.gw['a_to_b']['corrupted']} corrupted")
        measure.check(f"DEGRADED in the heartbeat after the {threshold}rd consecutive bad frame",
                      (t_deg - t_nth) * 1000.0 if t_deg and t_nth else float("nan"), "ms", 0, net.tick_ms + hb_period + 40)
        measure.check("RUNNING again after the first good frame",
                      (t_run - good_after) * 1000.0 if t_run and good_after else float("nan"), "ms", 0, net.tick_ms + hb_period + 40)
        measure.expect("no safe state (battery is not a critical input), SENSOR_DATA untouched",
                       fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "crc", "SENSOR_DATA") == 0)
        attempt.stalls = net.stalls(exp)
    measure.verify()

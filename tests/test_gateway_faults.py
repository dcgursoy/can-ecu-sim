"""Group 7: dropped, duplicated, delayed and reordered frames from the gateway.

Random bus-level faults, judged exactly rather than statistically: where a
fault can take more than one legitimate course (three lost SENSOR_DATA frames
in a row are a timeout, two are a counter gap), the expected outcome is
derived from the frames captured on the receiver's segment and the firmware's
counters are compared against it. Latency is measured per frame by matching
each frame on the sensor's segment with its copy on the controller's.
"""

from __future__ import annotations

import fault_scenarios as fs
from harness import FLAG, attempts

LOSSES = (("a_to_b", "SENSOR_DATA", "controller"), ("a_to_b", "SENSOR_HEARTBEAT", "controller"),
          ("b_to_a", "CONTROLLER_COMMAND", "sensor"))


def test_dropped_frames_are_accounted_by_the_counters(net, measure):
    """10 % random loss in both directions for 3 s"""
    cfg = {"drop": 0.10}
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults(cfg, cfg), 3.0, net.gw.faults, 1.6)
        events = []
        for direction, name, role in LOSSES:
            frames = exp.fb if direction == "a_to_b" else exp.fa
            key = f"0x{net.id[name]:03X}"
            dropped = exp.gw[direction]["by_id"].get(key, {}).get("dropped", 0)
            timeout_gaps, missing, ambiguous = fs.loss_expectations(frames, net.id[name], name, exp.t_apply, exp.t_clear + 0.1)
            lost = fs.rx_delta(exp.before, exp.after, role, "lost", name)
            timeouts = fs.rx_delta(exp.before, exp.after, role, "timeouts", name)
            measure.value(f"{name}: dropped by the gateway", dropped, "frames",
                          note=f"{role} counted {lost} lost by counter and {timeouts} timeouts; "
                               f"{len(timeout_gaps)} gaps over {net.timeout_ms(name)} ms hold {missing} frames")
            if ambiguous:
                measure.value(f"{name}: a gap within {fs.AMBIGUOUS_MS:.0f} ms of the timeout, accounting not judged", True)
                continue
            measure.expect(f"{name}: timeouts == gaps over the timeout", timeouts == len(timeout_gaps), f"{timeouts} vs {len(timeout_gaps)}")
            measure.expect(f"{name}: dropped == counted lost + frames inside timeout gaps", dropped == lost + missing,
                           f"{dropped} vs {lost} + {missing}")
            if name != "CONTROLLER_COMMAND":
                events += [(s, s + g / 1000.0) for s, g in timeout_gaps]
        expected_safe = fs.merged_count(events)
        entries = fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries")
        measure.expect(f"controller safe-state entries == loss events on its critical inputs ({expected_safe})",
                       entries == expected_safe, entries)
        measure.expect("no CRC faults on either side (loss is not corruption); both RUNNING again",
                       fs.rx_delta(exp.before, exp.after, "sensor", "crc") == 0 and fs.rx_delta(exp.before, exp.after, "controller", "crc") == 0
                       and fs.state_of(exp.after, "sensor") == "RUNNING" and fs.state_of(exp.after, "controller") == "RUNNING")
        measure.value("gateway: dropped sensor->controller / controller->sensor",
                      f"{exp.gw['a_to_b']['dropped']} of {exp.gw['a_to_b']['received']} / {exp.gw['b_to_a']['dropped']} of {exp.gw['b_to_a']['received']}")
        attempt.stalls = net.stalls(exp, exclude_relay=[(exp.t_apply - 0.05, exp.t_clear + 0.1)])
    measure.verify()


def test_duplicated_frames_are_rejected(net, measure):
    """20 % of the sensor->controller frames delivered twice for 2 s"""
    hb = net.msg["CONTROLLER_HEARTBEAT"]
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults({"duplicate": 0.2}), 2.0, net.gw.faults, 1.0)
        gw = exp.gw["a_to_b"]
        rejected = fs.rx_delta(exp.before, exp.after, "controller", "counter")
        flags_before = fs.decode_values(exp.fb, hb, "error_flags", exp.before.t, exp.t_apply)
        flags_after = fs.decode_values(exp.fb, hb, "error_flags", exp.t_apply, exp.after.t)
        measure.expect("controller repeated-counter rejections == frames the gateway duplicated", rejected == gw["duplicated"] > 0,
                       f"{rejected} rejected, {gw['duplicated']} duplicated of {gw['received']}")
        measure.expect("COUNTER_FAULT_SEEN clear before, published in the controller heartbeat after",
                       not any(int(v) & FLAG["COUNTER_FAULT_SEEN"] for _, v in flags_before)
                       and any(int(v) & FLAG["COUNTER_FAULT_SEEN"] for _, v in flags_after))
        measure.expect("no CRC faults, timeouts or safe state; both RUNNING",
                       fs.rx_delta(exp.before, exp.after, "controller", "crc") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                       and fs.state_of(exp.after, "controller") == "RUNNING" and fs.state_of(exp.after, "sensor") == "RUNNING")
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_delayed_frames_are_not_losses(net, measure):
    """15 +/- 5 ms of latency both ways, then a constant 20 ms (twice the SENSOR_DATA period)"""
    sd = net.id["SENSOR_DATA"]
    timeout = net.timeout_ms("SENSOR_DATA")
    for attempt in attempts(net, measure):
        cfg = {"delay_ms": 15, "jitter_ms": 5}
        exp = net.phase(lambda: net.gw.faults(cfg, cfg), 3.0, net.gw.faults, 1.0)
        lat = fs.latencies_ms(exp.fa, exp.fb, sd, exp.t_apply + 0.1, exp.t_clear - 0.05)
        measure.check("SENSOR_DATA latency p1 with 15 +/- 5 ms configured", fs.pct(lat, 1) if lat else float("nan"), "ms", lo=9.5,
                      note=f"n={len(lat)}")
        measure.check("SENSOR_DATA latency median", fs.pct(lat, 50) if lat else float("nan"), "ms", 13.0, 17.0)
        measure.check("SENSOR_DATA latency p99", fs.pct(lat, 99) if lat else float("nan"), "ms", hi=23.0)
        measure.value("gateway max scheduled latency a->b / b->a",
                      f"{exp.gw['a_to_b']['max_latency_ms']:.2f} / {exp.gw['b_to_a']['max_latency_ms']:.2f}", "ms")
        measure.expect("latency is not loss for a period-based timeout: no timeouts, no safe state, both RUNNING",
                       fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.rx_delta(exp.before, exp.after, "sensor", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                       and fs.state_of(exp.after, "controller") == "RUNNING" and fs.state_of(exp.after, "sensor") == "RUNNING")

        exp2 = net.phase(lambda: net.gw.faults({"delay_ms": 20}), 2.0, net.gw.faults, 1.0)
        lat2 = fs.latencies_ms(exp2.fa, exp2.fb, sd, exp2.t_apply + 0.1, exp2.t_clear - 0.05)
        gaps = fs.gaps(exp2.fb, sd, 15, exp2.t_apply - 0.05, exp2.t_clear + 0.1)
        measure.check("constant 20 ms delay: SENSOR_DATA latency median", fs.pct(lat2, 50) if lat2 else float("nan"), "ms", 19.0, 22.0)
        measure.check("constant 20 ms delay: largest SENSOR_DATA gap on the controller's segment (the step into the delay)",
                      max((g for _, g in gaps), default=0.0), "ms", hi=timeout - 1,
                      note=f"period 10 ms + 20 ms step, below the {timeout} ms timeout")
        measure.expect("20 ms of latency: no timeout, no counter fault, no safe state",
                       fs.rx_delta(exp2.before, exp2.after, "controller", "timeouts") == 0
                       and fs.rx_delta(exp2.before, exp2.after, "controller", "counter") == 0
                       and fs.delta(exp2.before, exp2.after, "controller", "node", "safe_state_entries") == 0)
        attempt.stalls = (net.stalls(exp, exclude_relay=[(exp.t_apply - 0.05, exp.t_clear + 0.1)])
                          + net.stalls(exp2, exclude_relay=[(exp2.t_apply - 0.05, exp2.t_clear + 0.1)]))
    measure.verify()


def test_reordered_frames_are_rejected_out_of_sequence(net, measure):
    """20 % of SENSOR_DATA swapped with their successor for 2 s"""
    sd = net.id["SENSOR_DATA"]
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: net.gw.faults({"reorder": 0.2, "ids": [hex(sd)]}), 2.0, net.gw.faults, 1.0)
        gw = exp.gw["a_to_b"]
        swaps = gw["reordered"] - gw["released_late"]
        oos = fs.rx_delta(exp.before, exp.after, "controller", "counter", "SENSOR_DATA")
        skipped = fs.rx_delta(exp.before, exp.after, "controller", "skipped", "SENSOR_DATA")
        lost = fs.rx_delta(exp.before, exp.after, "controller", "lost", "SENSOR_DATA")
        measure.value("SENSOR_DATA frames swapped with their successor", swaps, "frames",
                      note=f"{gw['released_late']} held frames released late without a successor")
        measure.expect("out-of-sequence rejections == swaps (the late frame is rejected)", oos == swaps > 0, oos)
        measure.expect("accepted-with-loss events == swaps (the successor arrives with counter +2), one frame counted lost each",
                       skipped == swaps and lost == swaps, f"{skipped} accepted with loss, {lost} lost")
        measure.expect("no CRC faults, timeouts or safe state; both RUNNING",
                       fs.rx_delta(exp.before, exp.after, "controller", "crc") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                       and fs.state_of(exp.after, "controller") == "RUNNING" and fs.state_of(exp.after, "sensor") == "RUNNING")
        attempt.stalls = net.stalls(exp, exclude_relay=[(exp.t_apply - 0.05, exp.t_clear + 0.1)])
    measure.verify()

"""Group 4: behaviour when one node floods the bus.

The gateway's flood generator plays a babbling node on the controller's
segment: frames with an identifier nobody expects, at a fixed rate. On a real
bus such a node steals bandwidth and, if its identifier wins arbitration,
delays everyone else; on vcan there is no arbitration, so what is tested is the
receiver: every frame reaches the controller's receive callback and queue,
where it must be counted as unknown and dropped by the application without
disturbing its own periodic output or its handling of the genuine traffic.
The rate sweep records where the receive queue stands against the flood.
"""

from __future__ import annotations

import fault_scenarios as fs
from harness import attempts, periods_ms, stats

FLOOD_ID = "0x001"
RATES = (1000, 2000, 4000)


def flood_phase(net, rate: int, hold_s: float, settle_s: float):
    holder: dict = {}

    def clear() -> None:
        holder["flood"] = net.gw.flood(None) or {}

    exp = net.phase(lambda: net.gw.flood({"iface": "b", "id": FLOOD_ID, "rate": rate, "dlc": 8}), hold_s, clear, settle_s)
    return exp, holder.get("flood", {})


def hwm(snap) -> str:
    return snap.controller.get("rx_summary", {}).get("queue_hwm", "?")


def test_unknown_frame_flood_is_counted_and_not_disruptive(net, measure):
    rate = 2000
    cmd = net.id["CONTROLLER_COMMAND"]
    nominal = net.period_ms("CONTROLLER_COMMAND")
    for attempt in attempts(net, measure):
        exp, fl = flood_phase(net, rate, 2.0, 0.5)
        sent = int(fl.get("sent", 0))
        achieved = float(fl.get("achieved_rate_hz", 0.0))
        unknown = fs.delta(exp.before, exp.after, "controller", "rx_summary", "unknown")
        dropped = fs.delta(exp.before, exp.after, "controller", "rx_summary", "dropped")
        during = stats(periods_ms(exp.fb, cmd, exp.t_apply + 0.1, exp.t_clear))
        after_flood = stats(periods_ms(exp.fb, cmd, exp.t_clear + 0.05, exp.after.t))
        accepted = fs.rx_delta(exp.before, exp.after, "controller", "ok", "SENSOR_DATA")
        span = exp.after.t - exp.before.t
        measure.check(f"flood rate achieved by the gateway ({rate}/s requested)", achieved, "frames/s", lo=rate * 0.9,
                      note=f"{sent} frames in {fl.get('span_s')} s, {fl.get('errors')} send errors; Python sender on a desktop OS")
        measure.check("flood frames counted as unknown by the controller", unknown / sent * 100.0 if sent else float("nan"), "%",
                      lo=90.0, note=f"{unknown} of {sent}")
        measure.expect("no frame dropped by a full receive queue", dropped == 0,
                       f"{dropped} dropped, queue high-water mark {hwm(exp.after)}")
        measure.value("receive queue high-water mark / depth after the flood", hwm(exp.after), "frames")
        measure.check("CONTROLLER_COMMAND period p99 during the flood", during.p99, "ms", nominal * 0.9, nominal * 1.1,
                      note=f"n={during.n}, max {during.max:.1f} ms")
        measure.check("CONTROLLER_COMMAND period max during the flood", during.max, "ms", hi=nominal * 1.25)
        measure.value("CONTROLLER_COMMAND period p99 after the flood", after_flood.p99, "ms",
                      note=f"n={after_flood.n}, max {after_flood.max:.1f} ms")
        measure.check("SENSOR_DATA accepted by the controller during the experiment", accepted / span, "frames/s",
                      1000.0 / net.period_ms("SENSOR_DATA") * 0.95, 1000.0 / net.period_ms("SENSOR_DATA") * 1.05)
        measure.expect("controller: no timeouts, no CRC or counter faults, no safe state, RUNNING",
                       fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "crc") == 0
                       and fs.rx_delta(exp.before, exp.after, "controller", "counter") == 0
                       and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                       and fs.state_of(exp.after, "controller") == "RUNNING")
        measure.expect("the flood stayed on its segment: the sensor saw no unknown frames, is RUNNING",
                       fs.delta(exp.before, exp.after, "sensor", "rx_summary", "unknown") == 0
                       and fs.state_of(exp.after, "sensor") == "RUNNING")
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_flood_rate_sweep(net, measure):
    """1 s at each rate: what the receive path absorbs before anything is dropped"""
    cmd = net.id["CONTROLLER_COMMAND"]
    nominal = net.period_ms("CONTROLLER_COMMAND")
    depth = net.cfg["RX_QUEUE_DEPTH"]
    for attempt in attempts(net, measure):
        stalls: list[str] = []
        for rate in RATES:
            exp, fl = flood_phase(net, rate, 1.0, 0.3)
            sent = int(fl.get("sent", 0))
            achieved = float(fl.get("achieved_rate_hz", 0.0))
            unknown = fs.delta(exp.before, exp.after, "controller", "rx_summary", "unknown")
            dropped = fs.delta(exp.before, exp.after, "controller", "rx_summary", "dropped")
            during = stats(periods_ms(exp.fb, cmd, exp.t_apply + 0.1, exp.t_clear))
            measure.value(f"{rate}/s requested: achieved", achieved, "frames/s", note=f"{sent} frames")
            measure.check(f"{rate}/s: counted as unknown", unknown / sent * 100.0 if sent else float("nan"), "%", lo=90.0)
            measure.expect(f"{rate}/s: nothing dropped by the {depth}-frame receive queue", dropped == 0,
                           f"{dropped} dropped, high-water mark {hwm(exp.after)}")
            measure.check(f"{rate}/s: CONTROLLER_COMMAND period max during the flood", during.max, "ms", hi=nominal * 1.25,
                          note=f"p99 {during.p99:.2f} ms")
            measure.expect(f"{rate}/s: no timeouts or safe state, RUNNING",
                           fs.rx_delta(exp.before, exp.after, "controller", "timeouts") == 0
                           and fs.delta(exp.before, exp.after, "controller", "node", "safe_state_entries") == 0
                           and fs.state_of(exp.after, "controller") == "RUNNING")
            stalls += net.stalls(exp)
        attempt.stalls = stalls
    measure.verify()

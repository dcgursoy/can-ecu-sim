"""Group 1: every periodic message stays within its timing tolerance.

Each message is measured on the segment where it is the sender's own output
(sensor messages on vcan0, controller messages on vcan1), so the relay is not
in the picture. The period is the time between consecutive frames of one
identifier, from candump's kernel timestamps. Limits, per message: the mean
within 1 % of the DBC cycle time, the 99th percentile within 10 %, every single
period within 25 % (the firmware's TX thread is a desktop process: an occasional
late wake-up is the host, a mean or p99 off is the scheduler), the rate over the
capture within 2 %. A gap longer than three periods is a host stall and makes
the attempt inconclusive instead.
"""

from __future__ import annotations

from harness import PERIODIC, attempts, periods_ms, stats, times_of

CAPTURE_S = 5.0


def test_periodic_messages_stay_within_tolerance(net, measure):
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: None, CAPTURE_S)
        t0, t1 = exp.t_apply, exp.t_clear
        for name in PERIODIC:
            frames = exp.frames(net.own_iface_is_a(name))
            nominal = net.period_ms(name)
            s = stats(periods_ms(frames, net.id[name], t0, t1))
            ts = times_of(frames, net.id[name], t0, t1)
            rate = (len(ts) - 1) / (ts[-1] - ts[0]) if len(ts) > 1 else 0.0
            measure.check(f"{name} period mean", s.mean, "ms", nominal * 0.99, nominal * 1.01,
                          note=f"n={s.n}, nominal {nominal:.0f} ms")
            measure.value(f"{name} period std", s.std, "ms")
            measure.check(f"{name} period p99", s.p99, "ms", nominal * 0.90, nominal * 1.10)
            measure.check(f"{name} period min", s.min, "ms", lo=nominal * 0.75)
            measure.check(f"{name} period max", s.max, "ms", hi=nominal * 1.25)
            measure.check(f"{name} rate", rate, "frames/s", 1000.0 / nominal * 0.98, 1000.0 / nominal * 1.02)
        for role in ("sensor", "controller"):
            tx = getattr(exp.after, role).get("tx", {})
            measure.expect(f"{role}: no TX tick overruns, nothing busy or rejected",
                           tx.get("tick_overruns") == "0" and tx.get("busy") == "0" and tx.get("rejected") == "0",
                           f"overruns {tx.get('tick_overruns')}, busy {tx.get('busy')}, rejected {tx.get('rejected')}")
            wdt = getattr(exp.after, role).get("wdt", {})
            measure.expect(f"{role}: watchdog fed on every supervisor tick", wdt.get("skipped") == "0",
                           f"feeds {wdt.get('feeds')}, skipped {wdt.get('skipped')}")
        attempt.stalls = net.stalls(exp)
    measure.verify()


def test_firmware_clock_tracks_bus_time(net, measure):
    """The uptime field of each heartbeat is the firmware's own clock; against the
    kernel timestamps of the same frames it shows whether the simulated clock
    keeps pace with host time (native_sim runs in real time: it should)."""
    for attempt in attempts(net, measure):
        exp = net.phase(lambda: None, 3.0)
        for name in ("SENSOR_HEARTBEAT", "CONTROLLER_HEARTBEAT"):
            frames = exp.frames(net.own_iface_is_a(name))
            msg = net.msg[name]
            points = []
            for t, i, data in frames:
                if i == msg.frame_id and exp.t_apply <= t <= exp.t_clear:
                    try:
                        points.append((t, int(msg.decode(bytes(data))["uptime"])))
                    except Exception:  # a truncated frame
                        continue
            if len(points) < 10:
                measure.expect(f"{name}: enough heartbeats captured", False, len(points))
                continue
            (t_first, up_first), (t_last, up_last) = points[0], points[-1]
            bus_ms = (t_last - t_first) * 1000.0
            fw_ms = float(up_last - up_first)
            ratio = fw_ms / bus_ms if bus_ms else float("nan")
            measure.check(f"{name}: firmware ms per host ms", ratio, "", 0.995, 1.005,
                          note=f"{fw_ms:.0f} ms of uptime over {bus_ms:.1f} ms of bus time, {len(points)} heartbeats")
            drift = [(up - up_first) - (t - t_first) * 1000.0 for t, up in points]
            measure.check(f"{name}: largest firmware-clock deviation from bus time", max(abs(d) for d in drift), "ms", hi=15.0,
                          note="heartbeat uptime is stamped at assembly, up to one TX slot before the frame")
        attempt.stalls = net.stalls(exp)
    measure.verify()

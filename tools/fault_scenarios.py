#!/usr/bin/env python3
"""Fault-injection scenarios on the gateway topology (inside the dev container).

    python3 tools/fault_scenarios.py [--scenarios NAME,NAME,...] [--seed 1] [--list]
    (common: --log-dir /tmp/can-ecu-sim-logs --retries N)

Topology:   sensor ECU -- vcan0 -- gateway/gateway.py -- vcan1 -- controller ECU
with candump on both segments. The gateway is driven over its JSON control
channel, the firmware hooks through TEST_CONTROL frames, and every scenario is
judged from three sources: each ECU's `ecu status` counters before and after
(deltas), the gateway's own counters, and the frames captured on each segment
(kernel timestamps). Where a random fault can take more than one legitimate
course (three lost SENSOR_DATA frames in a row are a timeout, two are not), the
expected outcome is derived from the captured frames, so the checks are exact
rather than statistical.

Scenarios, in the order they run:
    baseline            all traffic through the relay, rates and relay latency
    delay-jitter        15 +/- 5 ms latency both ways: invisible to period-based timeouts
    duplicate           20 % of sensor->controller frames twice: rejected as repeated counter
    reorder             20 % of SENSOR_DATA swapped with their successor: out-of-sequence rejects
    corrupt             one bit flipped in 10 % of sensor->controller frames: rejected by CRC
    corrupt-battery     every BATTERY_STATUS corrupted for 350 ms: DEGRADED on 3 consecutive rejects
    drop-heartbeat      every SENSOR_HEARTBEAT lost for 1 s: SAFE_STATE after 350 ms, recovery
    drop-random         10 % random loss both ways for 3 s: loss accounting vs counters and timeouts
    flood               2000 frames/s of id 0x001 on the controller's segment for 2 s
    hook-corrupt-crc    firmware CORRUPT_CRC x2 (tolerated) and x5 (timeout, safe state)
    hook-skip-counter   firmware SKIP_COUNTER x3: accepted with loss
    hook-freeze-counter firmware FREEZE_COUNTER x2: rejected as repeated
    hook-suppress-heartbeat  firmware SUPPRESS_HEARTBEAT 600 ms: safe state and recovery
    hook-suppress-message    firmware SUPPRESS_MESSAGE CONTROLLER_COMMAND 300 ms: sensor DEGRADED
    hook-stall-thread   firmware STALL_THREAD tx 1500 ms: watchdog resets the sensor node
docs/fault-model.md explains what each fault stands for on real hardware.

Verdicts and host-stall handling follow ecu_network.py: a gap in an ECU's own
periodic output on its own segment that no scenario explains is a stalled ECU
process; a gap in the relayed copy on the far segment only is a stalled gateway
process. Either makes the run INCONCLUSIVE (exit 3, repeated with --retries)
instead of FAIL. All numbers are from a simulated bus on a desktop OS.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import pathlib
import select
import subprocess
import sys
import time
import traceback
from typing import Callable

import cantools

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ecu_network as net  # noqa: E402

sys.path.insert(0, str(net.REPO / "can"))
import e2e_ref  # noqa: E402

IFACE_A = "vcan0"  # the sensor ECU's segment
IFACE_B = "vcan1"  # the controller ECU's segment
NODE_ALL, NODE_SENSOR, NODE_CONTROLLER = 0, 1, 2
CMD = {"RESET_FAULTS": 1, "SUPPRESS_HEARTBEAT": 4, "CORRUPT_CRC": 5, "SKIP_COUNTER": 6, "FREEZE_COUNTER": 7,
       "SUPPRESS_MESSAGE": 8, "STALL_THREAD": 9}
THREAD_TX = 1
FLAG_COUNTER_FAULT_SEEN = 1 << 1  # heartbeat error_flags bit (see the DBC comment)

# Firmware timeouts and periods (firmware/app/Kconfig, can/network.dbc) that decide
# what a run of lost or rejected frames means to the receiver.
TIMEOUT_MS = {"SENSOR_DATA": 35, "SENSOR_HEARTBEAT": 350, "BATTERY_STATUS": 350, "CONTROLLER_COMMAND": 70}
PERIOD_MS = {"SENSOR_DATA": 10, "SENSOR_HEARTBEAT": 100, "BATTERY_STATUS": 100, "CONTROLLER_COMMAND": 20}
AMBIGUOUS_MS = 4.0  # a gap this close to a timeout limit can fall either side of the firmware's own clock


# --- the gateway process ---------------------------------------------------------------

class GatewayProcess:
    """gateway/gateway.py between two interfaces, driven through its JSON control channel."""

    def __init__(self, iface_a: str, iface_b: str, log_dir: pathlib.Path, seed: int) -> None:
        self.iface_a = iface_a
        self.iface_b = iface_b
        self.seed = seed
        self.log_path = log_dir / "gateway.log"
        self.proc: subprocess.Popen | None = None
        self._log = None

    def start(self) -> None:
        self._log = open(self.log_path, "ab", buffering=0)
        self.proc = subprocess.Popen(
            [sys.executable, "-m", "gateway.gateway", "--a", self.iface_a, "--b", self.iface_b,
             "--seed", str(self.seed), "--control"],
            cwd=net.REPO, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self._log, text=True, bufsize=1)
        self.command({"stats": True})  # the reply proves the relay threads are up

    def command(self, cmd: dict, timeout_s: float = 3.0) -> dict:
        p = self.proc
        if p is None or p.poll() is not None or p.stdin is None or p.stdout is None:
            raise RuntimeError("gateway is not running")
        p.stdin.write(json.dumps(cmd) + "\n")
        p.stdin.flush()
        ready, _, _ = select.select([p.stdout], [], [], timeout_s)
        if not ready:
            raise TimeoutError(f"gateway: no reply to {cmd} within {timeout_s} s")
        line = p.stdout.readline()
        if not line:
            raise RuntimeError("gateway closed its control channel")
        reply = json.loads(line)
        if not reply.get("ok"):
            raise RuntimeError(f"gateway refused {cmd}: {reply.get('error')}")
        return reply

    def faults(self, a_to_b: dict | None = None, b_to_a: dict | None = None) -> None:
        """Replace both directions' faults (None or {} clears a direction)."""
        self.command({"a_to_b": a_to_b or {}, "b_to_a": b_to_a or {}})

    def flood(self, spec: dict | None) -> dict | None:
        """Start a flood (spec) or stop it (None); returns the flood counters."""
        return self.command({"flood": spec}).get("flood")

    def stats(self, reset: bool = False) -> dict:
        return self.command({"stats": "reset" if reset else True})["stats"]

    def stop(self) -> None:
        if self.proc is None:
            return
        try:
            if self.proc.poll() is None:
                self.command({"quit": True}, timeout_s=2.0)
                self.proc.wait(timeout=3.0)
        except (RuntimeError, TimeoutError, subprocess.TimeoutExpired, OSError):
            self.proc.kill()
            self.proc.wait()
        finally:
            for stream in (self.proc.stdin, self.proc.stdout):
                if stream is not None:
                    stream.close()
            if self._log is not None:
                self._log.close()
                self._log = None


# --- status and capture helpers -----------------------------------------------------------

def ival(status: dict, *path: str) -> int:
    node = status
    for key in path:
        if not isinstance(node, dict):
            return 0
        node = node.get(key, {})
    try:
        return int(node)
    except (TypeError, ValueError):
        return 0


def rx_sum(status: dict, key: str) -> int:
    return sum(ival(status, "rx", name, key) for name in status.get("rx", {}))


@dataclasses.dataclass
class Snapshot:
    t: float  # host time (same clock as the candump timestamps)
    sensor: dict
    controller: dict
    sensor_log: int  # firmware log sizes: a scenario reads only what it produced
    controller_log: int


def delta(before: Snapshot, after: Snapshot, role: str, *path: str) -> int:
    return ival(getattr(after, role), *path) - ival(getattr(before, role), *path)


def rx_delta(before: Snapshot, after: Snapshot, role: str, key: str, name: str | None = None) -> int:
    if name is not None:
        return delta(before, after, role, "rx", name, key)
    return rx_sum(getattr(after, role), key) - rx_sum(getattr(before, role), key)


def state_of(snap: Snapshot, role: str) -> str:
    return str(getattr(snap, role).get("node", {}).get("state"))


def gaps(frames, can_id: int, limit_ms: float, t0: float, t1: float) -> list[tuple[float, float]]:
    """(start, gap_ms) for consecutive frames of `can_id` in [t0, t1] further apart than limit_ms."""
    ts = [t for t, i, _ in frames if i == can_id and t0 <= t <= t1]
    return [(a, (b - a) * 1000.0) for a, b in zip(ts, ts[1:]) if (b - a) * 1000.0 > limit_ms]


def loss_expectations(frames, can_id: int, name: str, t0: float, t1: float):
    """What the receiver must have seen for `name` in the window: (timeout gaps,
    frames missing inside those gaps, ambiguous) where ambiguous means a gap fell
    within AMBIGUOUS_MS of the timeout and the firmware's own clock decides."""
    timeout = TIMEOUT_MS[name]
    period = PERIOD_MS[name]
    all_gaps = gaps(frames, can_id, period * 1.5, t0, t1)
    timeout_gaps = [(s, g) for s, g in all_gaps if g > timeout]
    missing = sum(int(round(g / period)) - 1 for _, g in timeout_gaps)
    ambiguous = any(abs(g - timeout) <= AMBIGUOUS_MS for _, g in all_gaps)
    return timeout_gaps, missing, ambiguous


def merged_count(intervals: list[tuple[float, float]], slack_s: float = 0.05) -> int:
    """Number of distinct events when overlapping or adjacent intervals are merged."""
    count = 0
    end = None
    for start, stop in sorted(intervals):
        if end is None or start > end + slack_s:
            count += 1
        end = stop if end is None else max(end, stop)
    return count


def decode_values(frames, msg, signal: str, t0: float, t1: float) -> list[tuple[float, object]]:
    out = []
    for t, i, data in frames:
        if i != msg.frame_id or not t0 <= t <= t1:
            continue
        try:
            v = msg.decode(bytes(data), decode_choices=True)[signal]
        except Exception:  # truncated or undecodable frame
            continue
        out.append((t, v if isinstance(v, int) else str(v)))
    return out


def first_at(values: list[tuple[float, object]], wanted: object, after: float | None) -> float | None:
    if after is None:
        return None
    return next((t for t, v in values if t > after and v == wanted), None)


def latencies_ms(src, dst, can_id: int, t0: float, t1: float) -> list[float]:
    """Per-frame relay latency: each source frame matched with the next destination
    frame carrying identical data (standard frames, no faults in the window)."""
    a = [(t, d) for t, i, d in src if i == can_id and t0 <= t <= t1]
    b = [(t, d) for t, i, d in dst if i == can_id and t0 <= t <= t1 + 0.2]
    out = []
    j = 0
    for t_src, data in a:
        while j < len(b) and b[j][0] < t_src:
            j += 1  # a relayed copy cannot precede its original
        k = j
        while k < len(b) and k < j + 4 and b[k][1] != data:
            k += 1
        if k < len(b) and b[k][1] == data:
            out.append((b[k][0] - t_src) * 1000.0)
            j = k + 1
    return out


def pct(values: list[float], p: float) -> float:
    s = sorted(values)
    return s[min(len(s) - 1, int(round(p / 100.0 * (len(s) - 1))))]


def bad_crc_runs(frames, can_id: int, t0: float, t1: float) -> list[tuple[float, float, int]]:
    """Runs of consecutive frames of `can_id` whose E2E CRC is wrong: (t_first, t_last, length)."""
    runs = []
    start = last = None
    length = 0
    for t, i, data in frames:
        if i != can_id or not t0 <= t <= t1:
            continue
        if not e2e_ref.crc_ok(can_id, bytes(data)):
            start = t if length == 0 else start
            last = t
            length += 1
        elif length:
            runs.append((start, last, length))
            length = 0
    if length:
        runs.append((start, last, length))
    return runs


def log_since(path: pathlib.Path, offset: int, *needles: str) -> list[str]:
    """Firmware log lines written after `offset` that contain any needle."""
    with open(path, "rb") as f:
        f.seek(offset)
        text = f.read().decode("utf-8", errors="replace")
    out = []
    for raw in text.splitlines():
        line = net.ANSI_RE.sub("", raw).replace(net.PROMPT, "").strip()
        if line and any(n in line for n in needles) and (not out or out[-1] != line):
            out.append(line)
    return out


def ms(seconds: float) -> str:
    return f"{seconds * 1000.0:.1f} ms"


# --- the run and its scenarios --------------------------------------------------------------

@dataclasses.dataclass
class Result:
    name: str
    lines: list[str] = dataclasses.field(default_factory=list)
    checks: list[tuple[str, bool]] = dataclasses.field(default_factory=list)
    # Windows in which an ECU's own output is deliberately silent (ignored by the host-stall
    # check) and in which sensor->controller faults may open gaps in the relayed SENSOR_DATA.
    exclude_own: list[tuple[float, float]] = dataclasses.field(default_factory=list)
    exclude_relay: list[tuple[float, float]] = dataclasses.field(default_factory=list)

    def line(self, text: str) -> None:
        self.lines.append(text)

    def check(self, name: str, ok: bool) -> None:
        self.checks.append((name, bool(ok)))

    @property
    def ok(self) -> bool:
        return all(ok for _, ok in self.checks)


class Run:
    def __init__(self, args, log_dir: pathlib.Path) -> None:
        self.args = args
        self.db = cantools.database.load_file(net.DBC)
        names = ("SENSOR_DATA", "BATTERY_STATUS", "SENSOR_HEARTBEAT", "CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT")
        self.msg = {name: self.db.get_message_by_name(name) for name in names}
        self.id = {name: m.frame_id for name, m in self.msg.items()}
        self.sensor = net.EcuProcess("sensor", IFACE_A, log_dir)
        self.controller = net.EcuProcess("controller", IFACE_B, log_dir)
        self.gw = GatewayProcess(IFACE_A, IFACE_B, log_dir, args.seed)
        self.dump_a = net.Candump(IFACE_A, log_dir / "bus0.log")
        self.dump_b = net.Candump(IFACE_B, log_dir / "bus1.log")

    def start(self) -> None:
        net.vcan_up(IFACE_A)
        net.vcan_up(IFACE_B)
        self.dump_a.start()
        self.dump_b.start()
        self.gw.start()
        self.controller.start()
        self.sensor.start()
        deadline = time.monotonic() + 6.0
        states: dict[str, str] = {}
        while time.monotonic() < deadline:
            time.sleep(0.25)
            try:
                states = {r: str(self.ecu(r).status().get("node", {}).get("state")) for r in ("sensor", "controller")}
            except TimeoutError:
                continue
            if all(s == "RUNNING" for s in states.values()):
                return
        raise RuntimeError(f"ECUs did not reach RUNNING through the gateway: {states}")

    def stop(self) -> None:
        self.sensor.stop()
        self.controller.stop()
        self.gw.stop()
        self.dump_a.stop()
        self.dump_b.stop()

    def ecu(self, role: str) -> net.EcuProcess:
        return self.sensor if role == "sensor" else self.controller

    def snapshot(self) -> Snapshot:
        return Snapshot(time.time(), self.sensor.status(), self.controller.status(),
                        self.sensor.log_path.stat().st_size, self.controller.log_path.stat().st_size)

    def frames(self, iface: str):
        return net.read_frames((self.dump_a if iface == IFACE_A else self.dump_b).log_path)

    def reset_faults(self) -> None:
        """TEST_CONTROL RESET_FAULTS to both nodes (the gateway relays it to vcan1)."""
        net.send_test_control(IFACE_A, NODE_ALL, CMD["RESET_FAULTS"])
        time.sleep(0.15)

    def phase(self, apply: Callable[[], None], hold_s: float, clear: Callable[[], None] | None = None,
              settle_s: float = 0.0) -> tuple[Snapshot, Snapshot, float, float]:
        """reset -> snapshot -> apply -> hold -> clear -> settle -> snapshot.
        Returns (before, after, t_apply, t_clear) in host time."""
        self.reset_faults()
        before = self.snapshot()
        self.gw.stats(reset=True)
        t_apply = time.time()
        apply()
        time.sleep(hold_s)
        t_clear = time.time()
        if clear is not None:
            clear()
        time.sleep(settle_s)
        after = self.snapshot()
        return before, after, t_apply, t_clear

    def check_unaffected(self, res: Result, before: Snapshot, after: Snapshot, role: str, what: str = "") -> None:
        faults = (rx_delta(before, after, role, "crc"), rx_delta(before, after, role, "counter"),
                  rx_delta(before, after, role, "timeouts"), delta(before, after, role, "node", "safe_state_entries"))
        res.check(f"{role} unaffected{what}: RUNNING, no CRC/counter faults, timeouts or safe-state entries",
                  state_of(after, role) == "RUNNING" and faults == (0, 0, 0, 0))

    def check_rates(self, res: Result, iface: str, frames, t0: float, t1: float, names: set[str] | None = None) -> None:
        rates = net.frame_rates([f for f in frames if t0 <= f[0] <= t1], self.db)
        for name, (count, measured, expected) in sorted(rates.items()):
            if expected is None or (names is not None and name not in names):
                continue
            res.line(f"{iface} {name:<20} {count:5d} frames {measured:6.1f}/s (expected {expected:.0f})")
            res.check(f"{iface} {name} rate within 10 %", abs(measured - expected) <= 0.10 * expected)


def sc_baseline(run: Run) -> Result:
    """all traffic through the relay: rates on both segments and the relay's own latency"""
    res = Result("baseline")
    before, after, t0, t1 = run.phase(lambda: None, 3.0)
    gw = run.gw.stats()
    fa, fb = run.frames(IFACE_A), run.frames(IFACE_B)
    run.check_rates(res, IFACE_A, fa, before.t, after.t)
    run.check_rates(res, IFACE_B, fb, before.t, after.t)
    for label, src, dst, name in (("SENSOR_DATA vcan0->vcan1", fa, fb, "SENSOR_DATA"),
                                  ("CONTROLLER_COMMAND vcan1->vcan0", fb, fa, "CONTROLLER_COMMAND")):
        lat = latencies_ms(src, dst, run.id[name], before.t, after.t)
        if lat:
            res.line(f"relay latency {label}: median {pct(lat, 50):.3f} ms, p99 {pct(lat, 99):.3f} ms, "
                     f"max {max(lat):.3f} ms (n={len(lat)}; Python relay on a desktop OS)")
            res.check(f"relay latency {label} median below 2 ms", pct(lat, 50) < 2.0)
        else:
            res.check(f"relay latency {label} measurable", False)
    for d in ("a_to_b", "b_to_a"):
        s = gw[d]
        res.line(f"gateway {d}: received {s['received']}, forwarded {s['forwarded']}, dropped {s['dropped']}, "
                 f"tx_errors {s['tx_errors']}")
        res.check(f"gateway {d} forwarded every frame", s["forwarded"] == s["received"] > 0 and s["dropped"] == 0
                  and s["tx_errors"] == 0)
    run.check_unaffected(res, before, after, "sensor")
    run.check_unaffected(res, before, after, "controller")
    return res


def sc_delay_jitter(run: Run) -> Result:
    """15 +/- 5 ms latency in both directions: period-based timeouts do not see latency"""
    res = Result("delay-jitter")
    cfg = {"delay_ms": 15, "jitter_ms": 5}
    before, after, t0, t1 = run.phase(lambda: run.gw.faults(cfg, cfg), 3.0, run.gw.faults, 1.0)
    gw = run.gw.stats()
    fa, fb = run.frames(IFACE_A), run.frames(IFACE_B)
    lat = latencies_ms(fa, fb, run.id["SENSOR_DATA"], t0 + 0.1, t1 - 0.05)
    if lat:
        res.line(f"SENSOR_DATA latency vcan0->vcan1 with 15 +/- 5 ms configured: p1 {pct(lat, 1):.1f} ms, "
                 f"median {pct(lat, 50):.1f} ms, p99 {pct(lat, 99):.1f} ms (n={len(lat)})")
        res.check("median latency between 13 and 17 ms", 13.0 <= pct(lat, 50) <= 17.0)
        res.check("p1 above 9.5 ms and p99 below 23 ms", pct(lat, 1) >= 9.5 and pct(lat, 99) <= 23.0)
    else:
        res.check("latency measurable", False)
    res.line(f"gateway max scheduled latency: a->b {gw['a_to_b']['max_latency_ms']:.2f} ms, "
             f"b->a {gw['b_to_a']['max_latency_ms']:.2f} ms")
    res.check("gateway never scheduled more than 20 ms", all(gw[d]["max_latency_ms"] <= 20.01 for d in ("a_to_b", "b_to_a")))
    run.check_rates(res, IFACE_B, fb, t0 + 0.1, t1, {"SENSOR_DATA", "SENSOR_HEARTBEAT", "BATTERY_STATUS"})
    run.check_rates(res, IFACE_A, fa, t0 + 0.1, t1, {"CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT"})
    run.check_unaffected(res, before, after, "sensor")
    run.check_unaffected(res, before, after, "controller")
    res.exclude_relay.append((t0 - 0.05, t1 + 0.1))  # the steps into and out of the delay stretch one gap
    return res


def sc_duplicate(run: Run) -> Result:
    """20 % of sensor->controller frames delivered twice: every copy rejected as a repeated counter"""
    res = Result("duplicate")
    before, after, t0, t1 = run.phase(lambda: run.gw.faults({"duplicate": 0.2}), 2.0, run.gw.faults, 1.0)
    gw = run.gw.stats()["a_to_b"]
    rejected = rx_delta(before, after, "controller", "counter")
    res.line(f"gateway duplicated {gw['duplicated']} of {gw['received']} sensor->controller frames; "
             f"controller rejected {rejected} frames as repeated counter")
    res.check("controller counter rejections == duplicated frames", rejected == gw["duplicated"] > 0)
    fb = run.frames(IFACE_B)
    flags_before = decode_values(fb, run.msg["CONTROLLER_HEARTBEAT"], "error_flags", before.t, t0)
    flags_after = decode_values(fb, run.msg["CONTROLLER_HEARTBEAT"], "error_flags", t0, after.t)
    res.check("COUNTER_FAULT_SEEN flag clear before and published in the controller heartbeat after",
              not any(int(v) & FLAG_COUNTER_FAULT_SEEN for _, v in flags_before)
              and any(int(v) & FLAG_COUNTER_FAULT_SEEN for _, v in flags_after))
    res.check("controller: no CRC faults, timeouts or safe-state entries, RUNNING",
              rx_delta(before, after, "controller", "crc") == 0 and rx_delta(before, after, "controller", "timeouts") == 0
              and delta(before, after, "controller", "node", "safe_state_entries") == 0
              and state_of(after, "controller") == "RUNNING")
    run.check_unaffected(res, before, after, "sensor")
    return res


def sc_reorder(run: Run) -> Result:
    """20 % of SENSOR_DATA swapped with their successor: one accepted with loss, one out of sequence"""
    res = Result("reorder")
    cfg = {"reorder": 0.2, "ids": [hex(run.id["SENSOR_DATA"])]}
    before, after, t0, t1 = run.phase(lambda: run.gw.faults(cfg), 2.0, run.gw.faults, 1.0)
    gw = run.gw.stats()["a_to_b"]
    swaps = gw["reordered"] - gw["released_late"]
    oos = rx_delta(before, after, "controller", "counter", "SENSOR_DATA")
    skipped = rx_delta(before, after, "controller", "skipped", "SENSOR_DATA")
    lost = rx_delta(before, after, "controller", "lost", "SENSOR_DATA")
    res.line(f"gateway swapped {swaps} SENSOR_DATA frames with their successor "
             f"({gw['released_late']} released late without one)")
    res.line(f"controller SENSOR_DATA: {skipped} accepted with loss ({lost} frames counted lost), "
             f"{oos} rejected out of sequence")
    res.check("out-of-sequence rejections == swaps", oos == swaps > 0)
    res.check("accepted-with-loss events == swaps (the successor arrives with counter +2)", skipped == swaps and lost == swaps)
    res.check("controller: no CRC faults, timeouts or safe-state entries, RUNNING",
              rx_delta(before, after, "controller", "crc") == 0 and rx_delta(before, after, "controller", "timeouts") == 0
              and delta(before, after, "controller", "node", "safe_state_entries") == 0
              and state_of(after, "controller") == "RUNNING")
    run.check_unaffected(res, before, after, "sensor")
    return res


def sc_corrupt(run: Run) -> Result:
    """one bit flipped in 10 % of sensor->controller frames: every one rejected by the E2E CRC"""
    res = Result("corrupt")
    before, after, t0, t1 = run.phase(lambda: run.gw.faults({"corrupt": 0.1}), 2.0, run.gw.faults, 1.5)
    gw = run.gw.stats()["a_to_b"]
    crc = rx_delta(before, after, "controller", "crc")
    res.line(f"gateway corrupted {gw['corrupted']} of {gw['received']} frames; controller rejected {crc} by CRC")
    res.check("controller CRC rejections == corrupted frames", crc == gw["corrupted"] > 0)
    # A run of rejected frames is silence to the timeout monitor: expectations from the capture.
    fb = run.frames(IFACE_B)
    events = []
    for name in ("SENSOR_DATA", "SENSOR_HEARTBEAT"):
        runs = bad_crc_runs(fb, run.id[name], t0, t1 + 0.1)
        longest = max((n for _, _, n in runs), default=0)
        timeouts = [(s, e) for s, e, n in runs if (n + 1) * PERIOD_MS[name] > TIMEOUT_MS[name]]
        events += timeouts
        res.line(f"{name}: {len(runs)} runs of consecutive corrupted frames, longest {longest}; "
                 f"{len(timeouts)} long enough for a timeout")
        res.check(f"{name} timeouts == runs longer than the timeout ({len(timeouts)})",
                  rx_delta(before, after, "controller", "timeouts", name) == len(timeouts))
    expected_safe = merged_count(events)
    res.check(f"controller safe_state_entries == {expected_safe} (timeout runs)",
              delta(before, after, "controller", "node", "safe_state_entries") == expected_safe)
    transitions = log_since(run.controller.log_path, before.controller_log, "node_state:")
    res.line("controller node transitions: " + ("; ".join(transitions) if transitions else "none"))
    res.check("controller RUNNING again", state_of(after, "controller") == "RUNNING")
    run.check_unaffected(res, before, after, "sensor")
    return res


def sc_corrupt_battery(run: Run) -> Result:
    """every BATTERY_STATUS corrupted for 350 ms: three consecutive rejects degrade the controller"""
    res = Result("corrupt-battery")
    cfg = {"corrupt": 1.0, "ids": [hex(run.id["BATTERY_STATUS"])]}
    before, after, t0, t1 = run.phase(lambda: run.gw.faults(cfg), 0.35, run.gw.faults, 1.0)
    gw = run.gw.stats()["a_to_b"]
    crc = rx_delta(before, after, "controller", "crc", "BATTERY_STATUS")
    transitions = log_since(run.controller.log_path, before.controller_log, "node_state:")
    res.line(f"{gw['corrupted']} consecutive BATTERY_STATUS frames corrupted; controller rejected {crc} by CRC")
    res.line("controller node transitions: " + ("; ".join(transitions) if transitions else "none"))
    res.check("CRC rejections == corrupted frames (3 or 4)", crc == gw["corrupted"] >= 3)
    res.check("controller DEGRADED on the third consecutive reject", any("-> DEGRADED" in t for t in transitions))
    res.check("no safe state (battery is not a critical input); RUNNING again",
              delta(before, after, "controller", "node", "safe_state_entries") == 0 and state_of(after, "controller") == "RUNNING")
    res.check("SENSOR_DATA untouched", rx_delta(before, after, "controller", "crc", "SENSOR_DATA") == 0)
    run.check_unaffected(res, before, after, "sensor")
    return res


def sc_drop_heartbeat(run: Run) -> Result:
    """every SENSOR_HEARTBEAT lost for 1 s: safe state after the 350 ms timeout, recovery after it returns"""
    res = Result("drop-heartbeat")
    cfg = {"drop": 1.0, "ids": [hex(run.id["SENSOR_HEARTBEAT"])]}
    before, after, t0, t1 = run.phase(lambda: run.gw.faults(cfg), 1.0, run.gw.faults, 1.6)
    gw = run.gw.stats()["a_to_b"]
    fb = run.frames(IFACE_B)
    hb = run.id["SENSOR_HEARTBEAT"]
    modes = decode_values(fb, run.msg["CONTROLLER_COMMAND"], "mode", before.t, after.t)
    last_hb = max((t for t, i, _ in fb if i == hb and t <= t1), default=None)
    t_safe = first_at(modes, "SAFE_STATE", last_hb)
    first_hb_after = min((t for t, i, _ in fb if i == hb and t >= t1), default=None)
    t_normal = first_at(modes, "NORMAL", first_hb_after)
    detection = (t_safe - last_hb) * 1000.0 if t_safe and last_hb else None
    recovery = (t_normal - first_hb_after) * 1000.0 if t_normal and first_hb_after else None
    res.line(f"gateway dropped {gw['dropped']} SENSOR_HEARTBEAT frames")
    res.line(f"SAFE_STATE command {detection:.1f} ms after the last delivered heartbeat (timeout 350 ms + supervisor tick + command period)"
             if detection is not None else "no SAFE_STATE command observed")
    res.line(f"NORMAL command {recovery:.1f} ms after the first heartbeat delivered again (500 ms hold + tick + period)"
             if recovery is not None else "no return to NORMAL observed")
    res.check("SAFE_STATE 340..420 ms after the last heartbeat", detection is not None and 340.0 <= detection <= 420.0)
    res.check("NORMAL 500..600 ms after the first heartbeat again", recovery is not None and 500.0 <= recovery <= 600.0)
    res.check("controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, RUNNING again",
              delta(before, after, "controller", "node", "safe_state_entries") == 1
              and rx_delta(before, after, "controller", "timeouts", "SENSOR_HEARTBEAT") == 1
              and state_of(after, "controller") == "RUNNING")
    res.check("gateway dropped 9..11 heartbeats in 1 s", 9 <= gw["dropped"] <= 11)
    run.check_unaffected(res, before, after, "sensor", " (commands keep coming in safe-state mode)")
    return res


def sc_drop_random(run: Run) -> Result:
    """10 % random loss in both directions for 3 s: the counter accounts for every lost frame"""
    res = Result("drop-random")
    cfg = {"drop": 0.10}
    before, after, t0, t1 = run.phase(lambda: run.gw.faults(cfg, cfg), 3.0, run.gw.faults, 1.6)
    gw = run.gw.stats()
    fa, fb = run.frames(IFACE_A), run.frames(IFACE_B)
    res.exclude_relay.append((t0 - 0.05, t1 + 0.1))
    events = []
    for direction, frames, name, role in (("a_to_b", fb, "SENSOR_DATA", "controller"),
                                          ("a_to_b", fb, "SENSOR_HEARTBEAT", "controller"),
                                          ("b_to_a", fa, "CONTROLLER_COMMAND", "sensor")):
        key = f"0x{run.id[name]:03X}"
        dropped = gw[direction]["by_id"].get(key, {}).get("dropped", 0)
        timeout_gaps, missing, ambiguous = loss_expectations(frames, run.id[name], name, t0, t1 + 0.1)
        lost = rx_delta(before, after, role, "lost", name)
        timeouts = rx_delta(before, after, role, "timeouts", name)
        res.line(f"{name}: {dropped} dropped by the gateway; {role} counted {lost} lost by counter and "
                 f"{timeouts} timeouts ({len(timeout_gaps)} gaps over {TIMEOUT_MS[name]} ms hold {missing} frames)")
        if ambiguous:
            res.line(f"  a {name} gap within {AMBIGUOUS_MS:.0f} ms of the timeout: accounting not checked")
            continue
        res.check(f"{name} timeouts == gaps over the timeout", timeouts == len(timeout_gaps))
        res.check(f"{name} dropped == counted lost + frames inside timeout gaps", dropped == lost + missing)
        if name != "CONTROLLER_COMMAND":
            events += [(s, s + g / 1000.0) for s, g in timeout_gaps]
    expected_safe = merged_count(events)
    res.check(f"controller safe_state_entries == {expected_safe} (loss events on critical inputs)",
              delta(before, after, "controller", "node", "safe_state_entries") == expected_safe)
    res.check("no CRC faults on either side (loss is not corruption)",
              rx_delta(before, after, "sensor", "crc") == 0 and rx_delta(before, after, "controller", "crc") == 0)
    res.check("both RUNNING again", state_of(after, "sensor") == "RUNNING" and state_of(after, "controller") == "RUNNING")
    res.line(f"gateway: a->b dropped {gw['a_to_b']['dropped']} of {gw['a_to_b']['received']}, "
             f"b->a dropped {gw['b_to_a']['dropped']} of {gw['b_to_a']['received']}")
    return res


def sc_flood(run: Run) -> Result:
    """2000 frames/s of id 0x001 on the controller's segment for 2 s: counted, not disruptive"""
    res = Result("flood")
    holder: dict = {}

    def clear() -> None:
        holder["flood"] = run.gw.flood(None) or {}

    before, after, t0, t1 = run.phase(lambda: run.gw.flood({"iface": "b", "id": "0x001", "rate": 2000, "dlc": 8}),
                                      2.0, clear, 0.5)
    fl = holder.get("flood", {})
    sent = int(fl.get("sent", 0))
    unknown = delta(before, after, "controller", "rx_summary", "unknown")
    dropped = delta(before, after, "controller", "rx_summary", "dropped")
    res.line(f"flood: {sent} frames in {fl.get('span_s')} s ({fl.get('achieved_rate_hz')} /s achieved of 2000 requested), "
             f"{fl.get('errors')} send errors")
    res.line(f"controller: {unknown} unknown frames counted, {dropped} dropped by a full receive queue, "
             f"queue high-water mark/depth {after.controller.get('rx_summary', {}).get('queue_hwm', '?')}")
    res.check("controller counted at least 90 % of the flood as unknown frames", sent > 0 and unknown >= 0.9 * sent)
    res.check("flood reached at least 1000 frames/s", float(fl.get("achieved_rate_hz", 0)) >= 1000.0)
    fb = run.frames(IFACE_B)
    run.check_rates(res, IFACE_B, fb, t0 + 0.1, t1, {"SENSOR_DATA", "CONTROLLER_COMMAND"})
    run.check_unaffected(res, before, after, "controller", " by the flood")
    res.check("flood stayed on its segment: the sensor saw no unknown frames",
              delta(before, after, "sensor", "rx_summary", "unknown") == 0)
    run.check_unaffected(res, before, after, "sensor")
    return res


def sc_hook_corrupt_crc(run: Run) -> Result:
    """firmware CORRUPT_CRC: two bad frames are rejected and tolerated, five are a timeout and a safe state"""
    res = Result("hook-corrupt-crc")
    before, after, t0, t1 = run.phase(lambda: net.send_test_control(IFACE_A, NODE_SENSOR, CMD["CORRUPT_CRC"], 2), 0.5)
    hooked, crc = delta(before, after, "sensor", "hooks", "corrupted"), rx_delta(before, after, "controller", "crc")
    res.line(f"CORRUPT_CRC x2: sensor corrupted {hooked} frames, controller rejected {crc} by CRC, "
             f"{rx_delta(before, after, 'controller', 'timeouts')} timeouts")
    res.check("x2: corrupted == rejected == 2", hooked == 2 and crc == 2)
    res.check("x2: no timeout, no safe state", rx_delta(before, after, "controller", "timeouts") == 0
              and delta(before, after, "controller", "node", "safe_state_entries") == 0)
    before, after, t0, t1 = run.phase(lambda: net.send_test_control(IFACE_A, NODE_SENSOR, CMD["CORRUPT_CRC"], 5), 2.0)
    hooked, crc = delta(before, after, "sensor", "hooks", "corrupted"), rx_delta(before, after, "controller", "crc")
    timeouts = rx_delta(before, after, "controller", "timeouts", "SENSOR_DATA")
    safe = delta(before, after, "controller", "node", "safe_state_entries")
    res.line(f"CORRUPT_CRC x5: sensor corrupted {hooked} frames, controller rejected {crc} by CRC, "
             f"SENSOR_DATA timeouts {timeouts}, safe_state_entries {safe} (40+ ms without an accepted frame)")
    res.check("x5: corrupted == rejected == 5", hooked == 5 and crc == 5)
    res.check("x5: one SENSOR_DATA timeout and one safe-state entry", timeouts == 1 and safe == 1)
    res.check("controller RUNNING again", state_of(after, "controller") == "RUNNING")
    return res


def sc_hook_skip_counter(run: Run) -> Result:
    """firmware SKIP_COUNTER x3: each frame arrives with counter +2, accepted with one frame counted lost"""
    res = Result("hook-skip-counter")
    before, after, t0, t1 = run.phase(lambda: net.send_test_control(IFACE_A, NODE_SENSOR, CMD["SKIP_COUNTER"], 3), 0.5)
    hooked = delta(before, after, "sensor", "hooks", "skipped")
    skipped, lost = rx_delta(before, after, "controller", "skipped"), rx_delta(before, after, "controller", "lost")
    res.line(f"sensor skipped {hooked} counter values; controller: {skipped} accepted with loss, {lost} frames counted lost, "
             f"{rx_delta(before, after, 'controller', 'counter')} rejected")
    res.check("skipped == accepted-with-loss == lost == 3", hooked == 3 and skipped == 3 and lost == 3)
    res.check("nothing rejected, no timeout, no safe state", rx_delta(before, after, "controller", "counter") == 0
              and rx_delta(before, after, "controller", "timeouts") == 0
              and delta(before, after, "controller", "node", "safe_state_entries") == 0)
    return res


def sc_hook_freeze_counter(run: Run) -> Result:
    """firmware FREEZE_COUNTER x2: a stuck counter is rejected as repeated, twice is tolerated"""
    res = Result("hook-freeze-counter")
    before, after, t0, t1 = run.phase(lambda: net.send_test_control(IFACE_A, NODE_SENSOR, CMD["FREEZE_COUNTER"], 2), 0.5)
    hooked = delta(before, after, "sensor", "hooks", "frozen")
    rejected = rx_delta(before, after, "controller", "counter")
    res.line(f"sensor repeated the previous counter on {hooked} frames; controller rejected {rejected} as repeated")
    res.check("frozen == rejected == 2", hooked == 2 and rejected == 2)
    res.check("no CRC fault, no timeout, no safe state", rx_delta(before, after, "controller", "crc") == 0
              and rx_delta(before, after, "controller", "timeouts") == 0
              and delta(before, after, "controller", "node", "safe_state_entries") == 0)
    return res


def sc_hook_suppress_heartbeat(run: Run) -> Result:
    """firmware SUPPRESS_HEARTBEAT 600 ms: the controller enters its safe state and recovers"""
    res = Result("hook-suppress-heartbeat")
    before, after, t0, t1 = run.phase(lambda: net.send_test_control(IFACE_A, NODE_SENSOR, CMD["SUPPRESS_HEARTBEAT"], 600), 2.0)
    fa, fb = run.frames(IFACE_A), run.frames(IFACE_B)
    hb = run.id["SENSOR_HEARTBEAT"]
    modes = decode_values(fb, run.msg["CONTROLLER_COMMAND"], "mode", before.t, after.t)
    last_hb = max((t for t, i, _ in fa if i == hb and t <= t0 + 0.4), default=None)
    t_safe = first_at(modes, "SAFE_STATE", last_hb)
    first_hb_after = min((t for t, i, _ in fa if i == hb and t > t0 + 0.4), default=None)
    t_normal = first_at(modes, "NORMAL", first_hb_after)
    detection = (t_safe - last_hb) * 1000.0 if t_safe and last_hb else None
    recovery = (t_normal - first_hb_after) * 1000.0 if t_normal and first_hb_after else None
    suppressed = delta(before, after, "sensor", "hooks", "suppressed")
    res.line(f"sensor suppressed {suppressed} heartbeats; heartbeat silence on vcan0 "
             f"{ms(first_hb_after - last_hb) if first_hb_after and last_hb else 'n/a'}")
    res.line(f"SAFE_STATE {detection:.1f} ms after the last heartbeat, NORMAL {recovery:.1f} ms after the first one again"
             if detection is not None and recovery is not None else "safe state or recovery not observed")
    res.check("sensor suppressed 5..7 heartbeats", 5 <= suppressed <= 7)
    res.check("SAFE_STATE 340..420 ms after the last heartbeat", detection is not None and 340.0 <= detection <= 420.0)
    res.check("NORMAL 500..600 ms after the first heartbeat again", recovery is not None and 500.0 <= recovery <= 600.0)
    res.check("controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, RUNNING again",
              delta(before, after, "controller", "node", "safe_state_entries") == 1
              and rx_delta(before, after, "controller", "timeouts", "SENSOR_HEARTBEAT") == 1
              and state_of(after, "controller") == "RUNNING")
    return res


def sc_hook_suppress_message(run: Run) -> Result:
    """firmware SUPPRESS_MESSAGE CONTROLLER_COMMAND 300 ms: the sensor times out and degrades, then recovers"""
    res = Result("hook-suppress-message")
    cmd_id = run.id["CONTROLLER_COMMAND"]
    before, after, t0, t1 = run.phase(
        lambda: net.send_test_control(IFACE_B, NODE_CONTROLLER, CMD["SUPPRESS_MESSAGE"], cmd_id, 300), 1.5)
    res.exclude_own.append((t0 - 0.05, t0 + 0.5))
    fb = run.frames(IFACE_B)
    silence = gaps(fb, cmd_id, 100, t0 - 0.05, after.t)
    suppressed = delta(before, after, "controller", "hooks", "suppressed")
    transitions = log_since(run.sensor.log_path, before.sensor_log, "node_state:", "timeout:")
    res.line(f"controller suppressed {suppressed} commands; CONTROLLER_COMMAND silence on vcan1: "
             + ", ".join(f"{g:.0f} ms" for _, g in silence))
    res.line("sensor log: " + ("; ".join(transitions) if transitions else "none"))
    res.check("controller suppressed 13..17 commands in 300 ms", 13 <= suppressed <= 17)
    res.check("one command gap of 290..340 ms on the bus", len(silence) == 1 and 290.0 <= silence[0][1] <= 340.0)
    res.check("sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again",
              rx_delta(before, after, "sensor", "timeouts", "CONTROLLER_COMMAND") == 1
              and any("-> DEGRADED" in t for t in transitions) and state_of(after, "sensor") == "RUNNING")
    res.check("sensor never in safe state (no actuator)", delta(before, after, "sensor", "node", "safe_state_entries") == 0)
    run.check_unaffected(res, before, after, "controller")
    return res


def sc_hook_stall_thread(run: Run) -> Result:
    """firmware STALL_THREAD tx 1500 ms: the liveness monitor withholds the feed and the watchdog resets the node"""
    res = Result("hook-stall-thread")
    run.reset_faults()
    before = run.snapshot()
    run.gw.stats(reset=True)
    t0 = time.time()
    net.send_test_control(IFACE_A, NODE_SENSOR, CMD["STALL_THREAD"], THREAD_TX, 1500)
    time.sleep(3.0)
    restarted = False
    if not run.sensor.running():
        run.sensor.start()  # the simulator exited on the reset instead of re-executing itself
        restarted = True
        time.sleep(1.5)
    after = run.snapshot()
    res.exclude_own.append((t0 - 0.05, t0 + 3.0))
    fa = run.frames(IFACE_A)
    silence = gaps(fa, run.id["SENSOR_DATA"], 100, t0 - 0.05, after.t)
    lines = log_since(run.sensor.log_path, before.sensor_log, "test hook", "watchdog", "Booting", "ecu role=")
    uptime = ival(after.sensor, "ecu", "uptime_ms")
    rebooted = any("Booting Zephyr" in line for line in lines) or uptime < (after.t - t0) * 1000.0
    res.line("SENSOR_DATA silence on vcan0: " + (", ".join(f"{g:.0f} ms" for _, g in silence) or "none"))
    res.line(f"sensor uptime after: {uptime} ms; process {'restarted by the test driver' if restarted else 'kept its pid'}")
    for line in lines:
        res.line("sensor log: " + line)
    res.check("sensor logged the stalled tx thread and the withheld watchdog feed",
              any("tx thread blocked" in line for line in lines) and any("watchdog not fed" in line for line in lines))
    res.check("watchdog reset the node in place (new boot banner, uptime restarted, same process)", rebooted and not restarted)
    res.check("sensor silent 1000..1400 ms (300 ms liveness window + 1000 ms watchdog timeout, then reboot)",
              len(silence) == 1 and 1000.0 <= silence[0][1] <= 1400.0)
    res.check("controller: safe state entered once, RUNNING again",
              delta(before, after, "controller", "node", "safe_state_entries") == 1 and state_of(after, "controller") == "RUNNING")
    res.check("sensor RUNNING again", state_of(after, "sensor") == "RUNNING")
    return res


SCENARIOS: dict[str, Callable[[Run], Result]] = {
    "baseline": sc_baseline,
    "delay-jitter": sc_delay_jitter,
    "duplicate": sc_duplicate,
    "reorder": sc_reorder,
    "corrupt": sc_corrupt,
    "corrupt-battery": sc_corrupt_battery,
    "drop-heartbeat": sc_drop_heartbeat,
    "drop-random": sc_drop_random,
    "flood": sc_flood,
    "hook-corrupt-crc": sc_hook_corrupt_crc,
    "hook-skip-counter": sc_hook_skip_counter,
    "hook-freeze-counter": sc_hook_freeze_counter,
    "hook-suppress-heartbeat": sc_hook_suppress_heartbeat,
    "hook-suppress-message": sc_hook_suppress_message,
    "hook-stall-thread": sc_hook_stall_thread,
}


def cmd_faults(args) -> int:
    log_dir = pathlib.Path(args.log_dir)
    names = [s.strip() for s in args.scenarios.split(",")] if args.scenarios else list(SCENARIOS)
    unknown = [n for n in names if n not in SCENARIOS]
    if unknown:
        print(f"unknown scenarios: {unknown}; known: {list(SCENARIOS)}")
        return net.EXIT_FAIL
    run = Run(args, log_dir)
    results: list[Result] = []
    final: dict[str, dict] = {}
    run.start()
    try:
        print(f"topology: sensor@{IFACE_A} <-> gateway (seed {args.seed}) <-> controller@{IFACE_B}; both RUNNING")
        for name in names:
            doc = (SCENARIOS[name].__doc__ or "").strip()
            print(f"=== {name}: {doc}")
            try:
                res = SCENARIOS[name](run)
            except Exception:
                res = Result(name)
                res.lines += traceback.format_exc().splitlines()
                res.check("scenario ran without an exception", False)
                run.gw.faults()
                run.gw.flood(None)
            for line in res.lines:
                print("  " + line)
            for cname, ok in res.checks:
                print(f"  [{'ok' if ok else 'FAIL'}] {cname}")
            results.append(res)
        final = {"sensor": run.sensor.status(), "controller": run.controller.status()}
    finally:
        run.stop()
    for role, st in final.items():
        net.print_status(role, st)

    # Host stalls: each ECU's own output on its own segment, outside the windows in
    # which a scenario silenced it. Gateway stalls: a gap in the relayed SENSOR_DATA
    # on vcan1 that vcan0 does not show, outside the windows with sensor->controller faults.
    fa, fb = run.frames(IFACE_A), run.frames(IFACE_B)
    sd, cmd = run.id["SENSOR_DATA"], run.id["CONTROLLER_COMMAND"]
    own = sorted([f for f in fa if f[1] == sd] + [f for f in fb if f[1] == cmd])
    exclude = [w for r in results for w in r.exclude_own]
    stalls = net.host_stalls(own, run.db, exclude)
    relay_exclude = exclude + [w for r in results for w in r.exclude_relay]
    own_gaps = {round(s, 3) for s, _ in gaps(fa, sd, 30, 0.0, float("inf"))}
    for start, gap_ms in gaps(fb, sd, 30, 0.0, float("inf")):
        if any(lo <= start <= hi for lo, hi in relay_exclude):
            continue
        if any(abs(start - s) < 0.02 for s in own_gaps):
            continue  # the sensor itself was silent (already a host stall, or excluded)
        stalls.append((start, gap_ms, "SENSOR_DATA relayed on vcan1 only (gateway process)"))

    print("=== summary")
    for r in results:
        first = r.lines[0] if r.lines else ""
        print(f"  {r.name:<24} {'ok  ' if r.ok else 'FAIL'}  {first}")
    fails = [f"{r.name}: {c}" for r in results for c, ok in r.checks if not ok]
    return net.verdict(fails, stalls)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    net.add_common_arguments(parser)
    parser.add_argument("--scenarios", help="comma-separated subset, in this order (default: all)")
    parser.add_argument("--seed", type=int, default=1, help="gateway random seed")
    parser.add_argument("--list", action="store_true", help="list the scenarios and exit")
    args = parser.parse_args()
    if args.list:
        for name, fn in SCENARIOS.items():
            print(f"{name:<24} {(fn.__doc__ or '').strip()}")
        return 0
    return net.execute(args, "faults", cmd_faults)


if __name__ == "__main__":
    sys.exit(main())

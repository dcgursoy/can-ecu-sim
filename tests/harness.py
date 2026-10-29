"""Harness of the system test suite: the network under test and the measurement log.

Topology (the Phase 3 one, so every fault the gateway can inject is available):

    sensor ECU -- vcan0 -- gateway/gateway.py -- vcan1 -- controller ECU

with candump on both segments. Every test judges from three sources, as the
Phase 3 scenarios do: the delta of each ECU's `ecu status` counters across the
experiment, the gateway's own counters, and the frames captured on each segment
with kernel timestamps. The ECU processes, the gateway driver and the capture
analysis helpers are the Phase 2/3 ones (tools/ecu_network.py and
tools/fault_scenarios.py); this module adds what a test suite needs on top: a
network that is started once per test module and reset between tests, an
`Experiment` (apply a fault, hold, clear, settle, with snapshots around it), the
host-stall classification that turns a stalled desktop process into a retry
instead of a failure, and the `Measure` log that keeps every measured value
next to its verdict.

All timing numbers are from a simulated bus on a desktop OS, not from
real-time hardware. The firmware's own constants (timeouts, hold times, backoff)
are read from the `.config` of the controller build, so the expectations follow
the build rather than a copy of the numbers.
"""

from __future__ import annotations

import dataclasses
import math
import pathlib
import re
import shutil
import sys
import time
from typing import Callable, Iterator

import cantools
import pytest

REPO = pathlib.Path(__file__).resolve().parents[1]
for p in (REPO, REPO / "tools"):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import ecu_network as net  # noqa: E402  (Phase 2: ECU processes, candump, frames, host stalls)
import fault_scenarios as fs  # noqa: E402  (Phase 3: gateway process, snapshots, capture analysis)
from host import analysis  # noqa: E402  (Phase 4: percentile, mean, stdev)

SIMULATED_BUS_NOTE = "measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware"

NODE_ALL, NODE_SENSOR, NODE_CONTROLLER = fs.NODE_ALL, fs.NODE_SENSOR, fs.NODE_CONTROLLER
CMD = dict(fs.CMD, NOP=0, INJECT_CAN_ERROR=2, FORCE_BUS_OFF=3)
THREAD_RX, THREAD_TX, THREAD_SUPERVISOR = 0, 1, 2
FLAG = {"CRC_FAULT_SEEN": 1 << 0, "COUNTER_FAULT_SEEN": 1 << 1, "RX_TIMEOUT_SEEN": 1 << 2,
        "BUS_OFF_SEEN": 1 << 3, "WATCHDOG_WARNING": 1 << 4, "TEST_HOOK_ACTIVE": 1 << 5}

PERIODIC = ("SENSOR_DATA", "BATTERY_STATUS", "SENSOR_HEARTBEAT", "CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT")
SENSOR_MESSAGES = ("SENSOR_DATA", "BATTERY_STATUS", "SENSOR_HEARTBEAT")
CONTROLLER_MESSAGES = ("CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT")

# Kconfig defaults (firmware/app/Kconfig), used only when no .config is found
# next to the controller executable (a build from another toolchain).
CONFIG_DEFAULTS = {
    "SUPERVISOR_TICK_MS": 5, "RX_QUEUE_DEPTH": 32,
    "SENSOR_DATA_TIMEOUT_MS": 35, "BATTERY_STATUS_TIMEOUT_MS": 350, "COMMAND_TIMEOUT_MS": 70,
    "HEARTBEAT_TIMEOUT_MS": 350, "SAFE_STATE_RECOVERY_MS": 500, "QUALITY_FAULT_THRESHOLD": 3,
    "BUSOFF_BACKOFF_INITIAL_MS": 100, "BUSOFF_BACKOFF_MAX_MS": 1600, "BUSOFF_RECOVERY_TIMEOUT_MS": 500,
    "BUSOFF_MAX_ATTEMPTS": 5, "BUSOFF_STABLE_MS": 10000, "WATCHDOG_TIMEOUT_MS": 1000, "LIVENESS_WINDOW_MS": 300,
}
TIMEOUT_KEY = {"SENSOR_DATA": "SENSOR_DATA_TIMEOUT_MS", "BATTERY_STATUS": "BATTERY_STATUS_TIMEOUT_MS",
               "SENSOR_HEARTBEAT": "HEARTBEAT_TIMEOUT_MS", "CONTROLLER_COMMAND": "COMMAND_TIMEOUT_MS",
               "CONTROLLER_HEARTBEAT": "HEARTBEAT_TIMEOUT_MS"}
SIM_RECOVERY_DELAY_MS = 3  # hal/can_bus.hpp kSimRecoveryDelayMs: the modelled 128 x 11 bit times

FW_LOG_RE = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d{3}),(\d{3})\]\s*<(\w+)>\s*(\S+):\s*(.*)$")


def firmware_config(exe: pathlib.Path) -> dict[str, int]:
    """CONFIG_APP_* integers from the .config next to a Zephyr executable."""
    cfg = dict(CONFIG_DEFAULTS)
    path = exe.parent / ".config"
    if path.exists():
        for line in path.read_text().splitlines():
            if line.startswith("CONFIG_APP_") and "=" in line:
                key, value = line[len("CONFIG_APP_"):].split("=", 1)
                try:
                    cfg[key] = int(value)
                except ValueError:
                    pass
    return cfg


@dataclasses.dataclass
class Options:
    iface_a: str
    iface_b: str
    sensor_exe: pathlib.Path
    controller_exe: pathlib.Path
    seed: int
    log_dir: pathlib.Path
    results_dir: pathlib.Path


@dataclasses.dataclass
class Experiment:
    """One apply/hold/clear/settle cycle with the evidence around it."""

    before: fs.Snapshot
    after: fs.Snapshot
    t_apply: float
    t_clear: float
    fa: list  # frames on the sensor's segment, whole capture so far
    fb: list  # frames on the controller's segment
    gw: dict  # gateway counters since the apply

    def frames(self, iface_is_a: bool) -> list:
        return self.fa if iface_is_a else self.fb


# --- capture helpers (host time = candump kernel timestamps = time.time()) ------------------

def periods_ms(frames, can_id: int, t0: float, t1: float) -> list[float]:
    ts = [t for t, i, _ in frames if i == can_id and t0 <= t <= t1]
    return [(b - a) * 1000.0 for a, b in zip(ts, ts[1:])]


@dataclasses.dataclass
class Stats:
    n: int
    mean: float
    std: float
    min: float
    max: float
    p50: float
    p99: float


def stats(values: list[float]) -> Stats:
    if not values:
        return Stats(0, math.nan, math.nan, math.nan, math.nan, math.nan, math.nan)
    return Stats(len(values), analysis.mean(values), analysis.stdev(values), min(values), max(values),
                 analysis.percentile(values, 50), analysis.percentile(values, 99))


def times_of(frames, can_id: int, t0: float = 0.0, t1: float = float("inf")) -> list[float]:
    return [t for t, i, _ in frames if i == can_id and t0 <= t <= t1]


def last_before(frames, can_id: int, t: float) -> float | None:
    return max((ts for ts in times_of(frames, can_id) if ts <= t), default=None)


def first_after(frames, can_id: int, t: float) -> float | None:
    return min((ts for ts in times_of(frames, can_id) if ts >= t), default=None)


def firmware_log(path: pathlib.Path, offset: int = 0, *needles: str) -> list[tuple[float, str]]:
    """(firmware uptime in seconds, cleaned line) for log lines after `offset` that
    contain any needle (all lines when none given). The shell echoes every log
    line twice on native_sim; consecutive duplicates are dropped."""
    out: list[tuple[float, str]] = []
    with open(path, "rb") as f:
        f.seek(offset)
        text = f.read().decode("utf-8", errors="replace")
    for raw in text.splitlines():
        line = net.ANSI_RE.sub("", raw).replace(net.PROMPT, "").strip()
        m = FW_LOG_RE.match(line)
        if not m:
            continue
        if needles and not any(n in line for n in needles):
            continue
        t = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + int(m.group(3)) + int(m.group(4)) / 1e3 + int(m.group(5)) / 1e6
        if out and out[-1][1] == line:
            continue
        out.append((t, line))
    return out


# --- the network under test -----------------------------------------------------------

class Network:
    def __init__(self, opts: Options, name: str) -> None:
        self.opts = opts
        self.name = name
        self.log_dir = opts.log_dir / name
        self.db = cantools.database.load_file(net.DBC)
        self.msg = {m: self.db.get_message_by_name(m) for m in PERIODIC}
        self.id = {m: self.msg[m].frame_id for m in PERIODIC}
        self.cfg = firmware_config(opts.controller_exe)
        self.sensor = net.EcuProcess("sensor", opts.iface_a, self.log_dir, exe=opts.sensor_exe)
        self.controller = net.EcuProcess("controller", opts.iface_b, self.log_dir, exe=opts.controller_exe)
        self.gw = fs.GatewayProcess(opts.iface_a, opts.iface_b, self.log_dir, opts.seed)
        self.dump_a = net.Candump(opts.iface_a, self.log_dir / "bus0.log")
        self.dump_b = net.Candump(opts.iface_b, self.log_dir / "bus1.log")
        self.started_at = 0.0
        self.boot_s = {"sensor": math.nan, "controller": math.nan}
        self.timeline: list[str] = []

    # --- configuration-derived expectations ---
    def period_ms(self, name: str) -> float:
        return float(self.msg[name].cycle_time)

    def timeout_ms(self, name: str) -> int:
        return self.cfg[TIMEOUT_KEY[name]]

    @property
    def tick_ms(self) -> int:
        return self.cfg["SUPERVISOR_TICK_MS"]

    def own_iface_is_a(self, name: str) -> bool:
        """The segment on which a message is the sender's own output (not relayed)."""
        return name in SENSOR_MESSAGES

    # --- lifecycle ---
    def start(self) -> None:
        shutil.rmtree(self.log_dir, ignore_errors=True)
        self.log_dir.mkdir(parents=True)
        net.vcan_up(self.opts.iface_a)
        net.vcan_up(self.opts.iface_b)
        self.dump_a.start()
        self.dump_b.start()
        self.gw.start()
        self.started_at = time.time()
        self.controller.start()
        self.sensor.start()
        states = self.wait_running(timeout_s=8.0)
        fa, fb = self.frames(True), self.frames(False)
        for role, frames, hb in (("sensor", fa, "SENSOR_HEARTBEAT"), ("controller", fb, "CONTROLLER_HEARTBEAT")):
            first = first_after(frames, self.id[hb], self.ecu(role).started_at)
            if first is not None:
                self.boot_s[role] = first - self.ecu(role).started_at
        self.note(f"network up: sensor@{self.opts.iface_a} <-> gateway (seed {self.opts.seed}) <-> "
                  f"controller@{self.opts.iface_b}; {states}; first heartbeat after start: "
                  f"sensor {self.boot_s['sensor'] * 1000:.0f} ms, controller {self.boot_s['controller'] * 1000:.0f} ms")

    # A gcov-instrumented process writes its coverage data on exit; give it time
    # before SIGKILL would lose the data (a normal build exits in milliseconds).
    STOP_TIMEOUT_S = 20.0

    def stop(self) -> None:
        self.sensor.stop(timeout_s=self.STOP_TIMEOUT_S)
        self.controller.stop(timeout_s=self.STOP_TIMEOUT_S)
        self.gw.stop()
        self.dump_a.stop()
        self.dump_b.stop()

    def ecu(self, role: str) -> net.EcuProcess:
        return self.sensor if role == "sensor" else self.controller

    def restart(self, role: str) -> float:
        """Start a new process for a role whose process was killed; returns its start time."""
        ecu = self.ecu(role)
        if ecu.running():
            ecu.stop(timeout_s=self.STOP_TIMEOUT_S)
        ecu.start()
        self.note(f"{role} restarted at {ecu.started_at:.3f}")
        return ecu.started_at

    def note(self, text: str) -> None:
        self.timeline.append(f"{time.time():.3f} {text}")

    # --- observation ---
    def status(self, role: str) -> dict:
        return self.ecu(role).status()

    def snapshot(self) -> fs.Snapshot:
        return fs.Snapshot(time.time(), self.sensor.status(), self.controller.status(),
                           self.sensor.log_path.stat().st_size, self.controller.log_path.stat().st_size)

    def frames(self, iface_is_a: bool) -> list:
        return net.read_frames((self.dump_a if iface_is_a else self.dump_b).log_path)

    def state(self, role: str) -> str:
        try:
            return str(self.status(role).get("node", {}).get("state"))
        except (TimeoutError, RuntimeError):
            return "n/a"

    def wait_running(self, timeout_s: float = 6.0, roles: tuple[str, ...] = ("sensor", "controller")) -> dict[str, str]:
        """Poll `ecu status` until every role is RUNNING (raises with the states otherwise)."""
        deadline = time.monotonic() + timeout_s
        states: dict[str, str] = {}
        while True:
            states = {r: self.state(r) for r in roles}
            if all(s == "RUNNING" for s in states.values()):
                return states
            if time.monotonic() > deadline:
                raise RuntimeError(f"ECUs not RUNNING within {timeout_s} s: {states}")
            time.sleep(0.1)

    # --- control ---
    def send_test_control(self, node: int, command: int | str, arg0: int = 0, arg1: int = 0,
                          iface: str | None = None) -> None:
        """TEST_CONTROL to one node on its own segment (the gateway relays it to the other)."""
        cmd = CMD[command] if isinstance(command, str) else command
        if iface is None:
            iface = self.opts.iface_b if node == NODE_CONTROLLER else self.opts.iface_a
        net.send_test_control(iface, node, cmd, arg0, arg1)

    def reset_faults(self) -> None:
        """Clear every injected fault: gateway faults and flood, firmware RESET_FAULTS to both nodes."""
        self.gw.faults()
        self.gw.flood(None)
        self.send_test_control(NODE_ALL, "RESET_FAULTS", iface=self.opts.iface_a)
        time.sleep(0.15)

    def phase(self, apply: Callable[[], None], hold_s: float, clear: Callable[[], None] | None = None,
              settle_s: float = 0.0, reset: bool = True) -> Experiment:
        """reset -> snapshot -> apply -> hold -> clear -> settle -> snapshot."""
        if reset:
            self.reset_faults()
            self.wait_running()
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
        gw = self.gw.stats()
        return Experiment(before, after, t_apply, t_clear, self.frames(True), self.frames(False), gw)

    # --- host stalls (a desktop process not scheduled; INCONCLUSIVE, not FAIL) ---
    def stalls(self, exp: Experiment, exclude_own: list[tuple[float, float]] | None = None,
               exclude_relay: list[tuple[float, float]] | None = None) -> list[str]:
        """Unexplained gaps in each ECU's own output on its own segment (a stalled ECU
        process) or in the relayed SENSOR_DATA on the controller's segment only (a
        stalled gateway process), between the snapshots and outside the given windows."""
        t0, t1 = exp.before.t, exp.after.t
        sd, cmd = self.id["SENSOR_DATA"], self.id["CONTROLLER_COMMAND"]
        own = sorted([f for f in exp.fa if f[1] == sd and t0 <= f[0] <= t1]
                     + [f for f in exp.fb if f[1] == cmd and t0 <= f[0] <= t1])
        found = net.host_stalls(own, self.db, list(exclude_own or []))
        own_gaps = {round(s, 3) for s, _ in fs.gaps(exp.fa, sd, 30, t0, t1)}
        for start, gap_ms in fs.gaps(exp.fb, sd, 30, t0, t1):
            if any(lo <= start <= hi for lo, hi in (exclude_relay or [])):
                continue
            if any(abs(start - s) < 0.02 for s in own_gaps):
                continue
            found.append((start, gap_ms, "SENSOR_DATA relayed on vcan1 only (gateway process)"))
        return [f"{name}: gap of {gap_ms:.1f} ms at {start:.3f}" for start, gap_ms, name in found]


# --- measurements -------------------------------------------------------------------------

def fmt(value) -> str:
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if math.isnan(value):
            return "n/a"
        return f"{value:.3f}" if abs(value) < 10 else f"{value:.1f}"
    return str(value)


@dataclasses.dataclass
class Row:
    name: str
    value: object
    unit: str
    limit: str
    ok: bool | None  # None: informational
    note: str = ""

    @property
    def verdict(self) -> str:
        return "info" if self.ok is None else ("ok" if self.ok else "FAIL")

    def text(self) -> str:
        s = f"{self.name} = {fmt(self.value)}{' ' + self.unit if self.unit else ''}"
        if self.limit:
            s += f"  [{self.limit}]"
        if self.note:
            s += f"  ({self.note})"
        return s

    def to_dict(self) -> dict:
        v = self.value
        if isinstance(v, float) and math.isnan(v):
            v = None
        return {"name": self.name, "value": v, "unit": self.unit, "limit": self.limit, "result": self.verdict,
                "note": self.note}


class Measure:
    """The measurement log of one test: every value next to its verdict. The
    verdicts are collected rather than asserted one by one, so a failing test
    still reports every number; verify() raises with all failures at the end."""

    def __init__(self, nodeid: str) -> None:
        self.nodeid = nodeid
        self.rows: list[Row] = []
        self.attempts = 1
        self.stall_notes: list[str] = []

    def value(self, name: str, value, unit: str = "", note: str = "") -> Row:
        row = Row(name, value, unit, "", None, note)
        self.rows.append(row)
        return row

    def check(self, name: str, value: float, unit: str, lo: float | None = None, hi: float | None = None,
              note: str = "") -> bool:
        if lo is not None and hi is not None:
            limit = f"{fmt(lo)}..{fmt(hi)}"
        elif lo is not None:
            limit = f">= {fmt(lo)}"
        elif hi is not None:
            limit = f"<= {fmt(hi)}"
        else:
            limit = ""
        ok = isinstance(value, (int, float)) and not (isinstance(value, float) and math.isnan(value))
        ok = ok and (lo is None or value >= lo) and (hi is None or value <= hi)
        self.rows.append(Row(name, value, unit, limit, ok, note))
        return ok

    def expect(self, name: str, ok: bool, value="", unit: str = "", note: str = "") -> bool:
        self.rows.append(Row(name, value, unit, "expected", bool(ok), note))
        return bool(ok)

    @property
    def failures(self) -> list[Row]:
        return [r for r in self.rows if r.ok is False]

    def reset(self) -> None:
        self.rows.clear()

    def verify(self) -> None:
        if self.failures:
            raise AssertionError("measured values outside their limits:\n  " + "\n  ".join(
                f"[FAIL] {r.text()}" for r in self.failures))


@dataclasses.dataclass
class Attempt:
    n: int
    stalls: list[str] = dataclasses.field(default_factory=list)


def attempts(network: Network, measure: Measure, retries: int = 2) -> Iterator[Attempt]:
    """Repeat an experiment while a host stall could explain its failed checks.

        for attempt in attempts(net, measure):
            exp = net.phase(...)
            measure.check(...)
            attempt.stalls = net.stalls(exp, ...)
        measure.verify()

    A failed check without a stall is a real failure (the loop ends); a passed
    attempt ends the loop too. After `retries` stalled attempts the test is
    skipped as inconclusive, the Phase 2/3 policy for desktop processes."""
    for n in range(1, retries + 2):
        attempt = Attempt(n)
        yield attempt
        if not measure.failures or not attempt.stalls:
            measure.attempts = n
            return
        measure.stall_notes.append(f"attempt {n}: host stall ({'; '.join(attempt.stalls)})")
        failed = [r.text() for r in measure.failures]
        if n <= retries:
            network.note(f"attempt {n} inconclusive, retrying: {'; '.join(attempt.stalls)}")
            measure.reset()
            network.reset_faults()
            network.wait_running()
            continue
        measure.attempts = n
        pytest.skip(f"inconclusive after {n} attempts, host stall each time: "
                    f"{'; '.join(attempt.stalls)}; failed checks: {failed}")

"""pytest plumbing of the system suite: options, the network fixtures and the
measurement log that prints every measured value next to its verdict and
writes build/test-results/measurements.{json,md}.

Run inside the dev container (vcan, the built ECUs and can-utils are needed):

    python3 -m pytest tests -v
    python3 -m pytest tests -v --sensor-exe path/to/other/sensor   # e.g. the Rust ECU (Phase 7)

tools/run-system-tests.sh wraps this with the JUnit report and, with
--coverage, instrumented firmware builds and their line coverage.
"""

from __future__ import annotations

import datetime as dt
import json
import pathlib
import platform
import shutil
import time

import pytest

from harness import REPO, SIMULATED_BUS_NOTE, Measure, Network, Options, fmt


def pytest_addoption(parser):
    g = parser.getgroup("can-ecu-sim", "system tests of the two-ECU network on vcan")
    g.addoption("--iface-a", default="vcan0", help="the sensor ECU's segment (default vcan0)")
    g.addoption("--iface-b", default="vcan1", help="the controller ECU's segment (default vcan1)")
    g.addoption("--sensor-exe", default=None,
                help="sensor ECU executable, started as EXE --can-if=IFACE (default build/native_sim_sensor/zephyr/zephyr.exe)")
    g.addoption("--controller-exe", default=None,
                help="controller ECU executable (default build/native_sim_controller/zephyr/zephyr.exe)")
    g.addoption("--seed", type=int, default=1, help="gateway random seed (default 1)")
    g.addoption("--log-dir", default="/tmp/can-ecu-sim-tests",
                help="where the ECU, gateway and candump logs are written during the run (default /tmp/...: "
                     "the simulator's console output is synchronous host I/O, keep it off a slow bind mount)")
    g.addoption("--results-dir", default=str(REPO / "build" / "test-results"),
                help="where the logs and measurements are copied at the end (default build/test-results)")


@pytest.fixture(scope="session")
def options(request) -> Options:
    get = request.config.getoption
    sensor = get("--sensor-exe") or str(REPO / "build" / "native_sim_sensor" / "zephyr" / "zephyr.exe")
    controller = get("--controller-exe") or str(REPO / "build" / "native_sim_controller" / "zephyr" / "zephyr.exe")
    opts = Options(get("--iface-a"), get("--iface-b"), pathlib.Path(sensor).resolve(),
                   pathlib.Path(controller).resolve(), get("--seed"), pathlib.Path(get("--log-dir")),
                   pathlib.Path(get("--results-dir")))
    for exe in (opts.sensor_exe, opts.controller_exe):
        if not exe.exists():
            pytest.exit(f"ECU executable not found: {exe} (build it with tools/build-ecu.sh native_sim <role>)", 2)
    return opts


@pytest.fixture(scope="module")
def network(request, options) -> Network:
    """One topology per test module: both ECUs, the gateway and both captures."""
    name = pathlib.Path(request.module.__file__).stem
    nw = Network(options, name)
    nw.start()
    _SESSION.networks.append(nw)
    try:
        yield nw
    finally:
        nw.stop()
        dest = options.results_dir / "system" / name
        shutil.rmtree(dest, ignore_errors=True)
        dest.mkdir(parents=True, exist_ok=True)
        for p in sorted(nw.log_dir.iterdir()):
            if p.suffix == ".log":
                shutil.copy(p, dest / p.name)
        (dest / "timeline.txt").write_text("\n".join(nw.timeline) + "\n")


@pytest.fixture
def net(network: Network, request) -> Network:
    """The network with every fault cleared and both ECUs RUNNING; after the
    test the faults are cleared again and a process a test killed is restarted."""
    network.reset_faults()
    network.wait_running()
    network.note(f"--- {request.node.nodeid}")
    yield network
    for role in ("sensor", "controller"):
        if not network.ecu(role).running():
            network.restart(role)
    network.reset_faults()
    try:
        network.wait_running(timeout_s=8.0)
    except RuntimeError as e:
        network.note(f"after {request.node.name}: {e}")


@pytest.fixture
def measure(request) -> Measure:
    m = Measure(request.node.nodeid)
    _SESSION.measures[request.node.nodeid] = m
    yield m
    for i, row in enumerate(m.rows):
        request.node.user_properties.append((f"measurement_{i:02d}", f"[{row.verdict}] {row.text()}"))


# --- the measurement log ---------------------------------------------------------------------

class _Session:
    def __init__(self) -> None:
        self.measures: dict[str, Measure] = {}
        self.outcomes: dict[str, str] = {}
        self.order: list[str] = []
        self.networks: list[Network] = []
        self.started = time.time()
        self.config = None


_SESSION = _Session()


def _results_dir(config) -> pathlib.Path:
    return pathlib.Path(config.getoption("--results-dir"))


@pytest.hookimpl(trylast=True)
def pytest_runtest_logreport(report):
    if report.when == "setup" and report.nodeid not in _SESSION.order:
        _SESSION.order.append(report.nodeid)
    if report.when == "call" or (report.when == "setup" and report.outcome != "passed"):
        _SESSION.outcomes[report.nodeid] = report.outcome
    if report.when != "call":
        return
    m = _SESSION.measures.get(report.nodeid)
    if m is None or not m.rows or _SESSION.config is None:
        return
    tr = _SESSION.config.pluginmanager.get_plugin("terminalreporter")
    if tr is None or _SESSION.config.option.verbose <= 0:
        return
    tr.ensure_newline()
    for row in m.rows:
        tr.write_line(f"      [{row.verdict:<4}] {row.text()}")
    for note in m.stall_notes:
        tr.write_line(f"      [note] {note}")


def pytest_configure(config):
    _SESSION.config = config
    config.addinivalue_line("markers", "system: drives the two-ECU network on vcan (dev container only)")


def pytest_collection_modifyitems(items):
    for item in items:
        item.add_marker(pytest.mark.system)


def pytest_terminal_summary(terminalreporter, exitstatus, config):
    if not _SESSION.order:
        return
    tr = terminalreporter
    tr.section(f"measured values ({SIMULATED_BUS_NOTE})")
    for nodeid in _SESSION.order:
        m = _SESSION.measures.get(nodeid)
        outcome = _SESSION.outcomes.get(nodeid, "?")
        tr.write_line(f"{outcome.upper():<8} {nodeid}" + (f"  (attempts: {m.attempts})" if m and m.attempts > 1 else ""))
        if m is None:
            continue
        for row in m.rows:
            tr.write_line(f"    [{row.verdict:<4}] {row.text()}")
        for note in m.stall_notes:
            tr.write_line(f"    [note] {note}")
    out = _write_results(config)
    tr.write_line(f"measurements written to {out}")


def _write_results(config) -> pathlib.Path:
    results_dir = _results_dir(config)
    results_dir.mkdir(parents=True, exist_ok=True)
    tests = []
    for nodeid in _SESSION.order:
        m = _SESSION.measures.get(nodeid)
        tests.append({
            "test": nodeid,
            "outcome": _SESSION.outcomes.get(nodeid, "?"),
            "attempts": m.attempts if m else 1,
            "inconclusive_attempts": list(m.stall_notes) if m else [],
            "measurements": [r.to_dict() for r in m.rows] if m else [],
        })
    nw = _SESSION.networks[0] if _SESSION.networks else None
    doc = {
        "note": SIMULATED_BUS_NOTE,
        "date": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "duration_s": round(time.time() - _SESSION.started, 1),
        "host": {"platform": platform.platform(), "python": platform.python_version()},
        "options": {k: str(v) for k, v in vars(nw.opts).items()} if nw else {},
        "firmware_config": nw.cfg if nw else {},
        "boot_to_first_heartbeat_s": {m.name: dict(m.boot_s) for m in _SESSION.networks},
        "tests": tests,
    }
    (results_dir / "measurements.json").write_text(json.dumps(doc, indent=2) + "\n")

    lines = [f"# Measured values", "", f"_{SIMULATED_BUS_NOTE}_", "",
             f"Run {doc['date']}, {doc['duration_s']} s, seed {doc['options'].get('seed', '?')}.", "",
             "| test | measurement | value | limit | result |", "|---|---|---|---|---|"]
    for t in tests:
        short = t["test"].split("::", 1)[-1]
        for r in t["measurements"]:
            value = "n/a" if r["value"] is None else fmt(r["value"])
            unit = f" {r['unit']}" if r["unit"] else ""
            note = f" ({r['note']})" if r["note"] else ""
            lines.append(f"| {short} | {r['name']}{note} | {value}{unit} | {r['limit']} | {r['result']} |")
        if not t["measurements"]:
            lines.append(f"| {short} | | | | {t['outcome']} |")
    (results_dir / "measurements.md").write_text("\n".join(lines) + "\n")
    return results_dir / "measurements.json"

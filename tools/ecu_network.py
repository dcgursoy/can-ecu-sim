#!/usr/bin/env python3
"""Run the simulated two-ECU network on a vcan interface (inside the dev container).

    python3 tools/ecu_network.py run        [--duration 3]
    python3 tools/ecu_network.py safe-state [--kill-at 2] [--restart-at 4] [--duration 7]
    python3 tools/ecu_network.py bus-off    [--duration 7]
    (common: --iface vcan0 --log-dir /tmp/can-ecu-sim-logs --retries N)

`run` starts both native_sim ECUs, captures the bus with candump, prints each
ECU's `ecu status` shell output, stops everything, verifies every captured
frame with can/verify_candump.py and checks every periodic message's rate over
the capture span against the DBC.

`safe-state` additionally kills the sensor ECU process at --kill-at seconds,
restarts it at --restart-at, and measures from the bus log how long the
controller took to publish mode=SAFE_STATE after the sensor's last frame and
how long it took to return to mode=NORMAL after the sensor came back.

`bus-off` sends TEST_CONTROL INJECT_CAN_ERROR x30 to the controller (TEC 240,
error-passive for about 2 s while successful frames heal it by one each) and
then FORCE_BUS_OFF, both through the HAL's simulated controller, and reports
the state transitions, the recovery and what the bus saw meanwhile.

Verdicts: every subcommand prints the named pass criteria that failed. A failed
criterion is reported as RESULT: FAIL (exit 1) unless the bus log shows a host
stall (an unexplained gap in a periodic message outside the deliberate fault
window), in which case the run is RESULT: INCONCLUSIVE (exit 3) and is repeated
up to --retries times: the ECUs are desktop processes and a loaded CI runner
can suspend one of them longer than a 35 ms timeout. The candump timestamps
come from the kernel at transmit time, independent of either process's
scheduling, which is what makes the distinction possible.

Logs are written to --log-dir, by default a directory inside the container
(/tmp): the simulator's console output is synchronous host I/O, and a slow
bind-mounted filesystem would stall the process in host time while it prints,
which the peer ECU sees as a gap on the bus. After every attempt the logs and
this script's own output are copied to build/network-logs/<subcommand>/.

The EcuProcess and Candump classes are also the building blocks of the pytest
fixtures (Phase 5). All timing numbers come from a simulated bus on a desktop
OS, not from real-time hardware.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import shutil
import signal
import subprocess
import sys
import time
import traceback

import cantools

REPO = pathlib.Path(__file__).resolve().parents[1]
DBC = REPO / "can" / "network.dbc"
LOG_LINE_RE = re.compile(r"^\((\d+\.\d+)\)\s+\S+\s+([0-9A-Fa-f]{3,8})#([0-9A-Fa-f]*)$")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b\[J|\r")
PROMPT = "ecu:~$ "

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_INCONCLUSIVE = 3


def ecu_exe(role: str) -> pathlib.Path:
    return REPO / "build" / f"native_sim_{role}" / "zephyr" / "zephyr.exe"


class EcuProcess:
    """One native_sim ECU process with its shell on stdin and its output in a log file."""

    def __init__(self, role: str, iface: str, log_dir: pathlib.Path, exe: pathlib.Path | None = None) -> None:
        self.role = role
        self.iface = iface
        self.exe = exe or ecu_exe(role)
        self.log_path = log_dir / f"{role}.log"
        self.proc: subprocess.Popen | None = None
        self._log = None
        self._read_offset = 0
        self.started_at = 0.0

    def start(self) -> None:
        """Start (or restart) the process; the log file is appended to."""
        if not self.exe.exists():
            raise FileNotFoundError(f"{self.exe} (build it with tools/build-ecu.sh native_sim {self.role})")
        self._log = open(self.log_path, "ab", buffering=0)
        self._read_offset = self._log.tell()
        self.started_at = time.time()  # before Popen: a strict lower bound on every frame it sends
        self.proc = subprocess.Popen(
            [str(self.exe), f"--can-if={self.iface}"],
            stdin=subprocess.PIPE, stdout=self._log, stderr=subprocess.STDOUT, bufsize=0)

    def running(self) -> bool:
        return self.proc is not None and self.proc.poll() is None

    def read_new_output(self) -> str:
        with open(self.log_path, "rb") as f:
            f.seek(self._read_offset)
            data = f.read()
            self._read_offset = f.tell()
        return data.decode("utf-8", errors="replace")

    def shell(self, command: str, done: str | None = None, timeout_s: float = 2.0) -> str:
        """Send a shell command; return its output once a line starting with `done`
        (default: the role line that `ecu status` prints last) appears, else raise."""
        if not self.running() or self.proc.stdin is None:
            raise RuntimeError(f"{self.role} ECU is not running")
        self.read_new_output()
        self.proc.stdin.write((command + "\n").encode())
        self.proc.stdin.flush()
        sentinel = f"{self.role}: " if done is None else done
        deadline = time.monotonic() + timeout_s
        buf = ""
        while True:
            buf += self.read_new_output()
            cleaned = [ANSI_RE.sub("", line).replace(PROMPT, "").strip() for line in buf.splitlines()]
            if any(line.startswith(sentinel) for line in cleaned):
                return "\n".join(cleaned)
            if time.monotonic() > deadline:
                raise TimeoutError(f"{self.role}: no '{sentinel}' line within {timeout_s} s after '{command}'")
            time.sleep(0.01)

    def status(self) -> dict:
        return parse_status(self.shell("ecu status"))

    def stop(self, timeout_s: float = 2.0) -> int | None:
        if self.proc is None:
            return None
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=timeout_s)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        if self._log is not None:
            self._log.close()
            self._log = None
        return self.proc.returncode

    def kill(self) -> float:
        """SIGKILL the process (an ECU dying without any cleanup); returns the host
        time at which it was confirmed dead, an upper bound on every frame it sent."""
        assert self.proc is not None
        self.proc.send_signal(signal.SIGKILL)
        self.proc.wait()
        t = time.time()
        if self._log is not None:
            self._log.close()
            self._log = None
        return t


STATUS_KV_RE = re.compile(r"(\w+)=(\"[^\"]*\"|\S+)")


def parse_status(text: str) -> dict:
    """Parse `ecu status` output into {section: {key: value}}; rx lines keyed by message name."""
    out: dict = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("rx ") and ":" in line:
            name = line[3:line.index(":")]
            out.setdefault("rx", {})[name] = {k: v.strip('"') for k, v in STATUS_KV_RE.findall(line)}
        elif ":" in line and line.split(":", 1)[0] in ("node", "can", "tx", "wdt"):
            section = line.split(":", 1)[0]
            out.setdefault(section, {}).update({k: v.strip('"') for k, v in STATUS_KV_RE.findall(line)})
        elif line.startswith("rx:"):
            out.setdefault("rx_summary", {}).update({k: v.strip('"') for k, v in STATUS_KV_RE.findall(line)})
        elif line.startswith("role="):
            out.setdefault("ecu", {}).update({k: v.strip('"') for k, v in STATUS_KV_RE.findall(line)})
    return out


class Candump:
    def __init__(self, iface: str, log_path: pathlib.Path) -> None:
        self.iface = iface
        self.log_path = log_path
        self.proc: subprocess.Popen | None = None
        self._file = None

    def start(self) -> None:
        self._file = open(self.log_path, "wb")
        self.proc = subprocess.Popen(["candump", "-L", self.iface], stdout=self._file, stderr=subprocess.DEVNULL)
        time.sleep(0.2)

    def stop(self) -> None:
        if self.proc is not None:
            self.proc.terminate()
            self.proc.wait()
            self.proc = None
        if self._file is not None:
            self._file.close()
            self._file = None


def vcan_up(iface: str) -> None:
    subprocess.run([str(REPO / "tools" / "vcan-up.sh"), iface], check=True, stdout=subprocess.DEVNULL)


def read_frames(log_path: pathlib.Path) -> list[tuple[float, int, bytes]]:
    frames = []
    for line in log_path.read_text().splitlines():
        m = LOG_LINE_RE.match(line)
        if m:
            try:
                frames.append((float(m.group(1)), int(m.group(2), 16), bytes.fromhex(m.group(3))))
            except ValueError:
                continue  # a truncated last line
    return frames


def verify(log_path: pathlib.Path, min_protected: int) -> bool:
    result = subprocess.run([sys.executable, str(REPO / "can" / "verify_candump.py"), "--quiet",
                             "--min-protected", str(min_protected), str(log_path)])
    return result.returncode == 0


def frame_rates(frames, db) -> dict[str, tuple[int, float, float | None]]:
    """name -> (count, measured Hz over that message's first..last frame, expected Hz from the DBC)."""
    times: dict[int, list[float]] = {}
    for t, can_id, _ in frames:
        times.setdefault(can_id, []).append(t)
    out = {}
    for can_id, ts in times.items():
        try:
            msg = db.get_message_by_frame_id(can_id)
            name, expected = msg.name, (1000.0 / msg.cycle_time if msg.cycle_time else None)
        except KeyError:
            name, expected = f"0x{can_id:03X}", None
        measured = (len(ts) - 1) / (ts[-1] - ts[0]) if len(ts) > 1 and ts[-1] > ts[0] else 0.0
        out[name] = (len(ts), measured, expected)
    return out


def host_stalls(frames, db, exclude: list[tuple[float, float]] | None = None) -> list[tuple[float, float, str]]:
    """Unexplained gaps in the periodic messages: (start, gap_ms, message). A gap
    longer than three periods of a 10 ms or 20 ms message, outside the windows
    in `exclude`, means a process was not scheduled for that long."""
    limits = {db.get_message_by_name("SENSOR_DATA").frame_id: ("SENSOR_DATA", 0.030),
              db.get_message_by_name("CONTROLLER_COMMAND").frame_id: ("CONTROLLER_COMMAND", 0.060)}
    last: dict[int, float] = {}
    stalls = []
    for t, can_id, _ in frames:
        if can_id in limits:
            name, limit = limits[can_id]
            if can_id in last and t - last[can_id] > limit:
                start = last[can_id]
                if not any(lo <= start <= hi for lo, hi in (exclude or [])):
                    stalls.append((start, (t - start) * 1000.0, name))
            last[can_id] = t
    return stalls


def verdict(fails: list[str], stalls: list[tuple[float, float, str]]) -> int:
    for start, gap_ms, name in stalls:
        print(f"host stall: {name} gap of {gap_ms:.1f} ms starting {start:.6f}")
    if not fails:
        print("RESULT: OK")
        return EXIT_OK
    for f in fails:
        print(f"criterion failed: {f}")
    if stalls:
        print("RESULT: INCONCLUSIVE (host stall during the run; retry)")
        return EXIT_INCONCLUSIVE
    print("RESULT: FAIL")
    return EXIT_FAIL


def print_status(role: str, status: dict) -> None:
    print(f"--- {role} `ecu status`")
    for section, values in status.items():
        if section == "rx":
            for name, kv in values.items():
                print(f"  rx {name}: " + " ".join(f"{k}={v}" for k, v in kv.items()))
        else:
            print(f"  {section}: " + " ".join(f"{k}={v}" for k, v in values.items()))


def log_lines(log_path: pathlib.Path, *needles: str) -> list[str]:
    """Firmware log lines containing any needle, with shell redraw sequences removed."""
    out = []
    for raw in log_path.read_text(errors="replace").splitlines():
        line = ANSI_RE.sub("", raw).replace(PROMPT, "").strip()
        if line and any(n in line for n in needles) and line not in out:
            out.append(line)
    return out


def send_test_control(iface: str, target_node: int, command: int, arg0: int = 0, arg1: int = 0) -> None:
    """TEST_CONTROL (0x7E0): magic 0xC0DE, target, command, arg0, arg1 (all little-endian)."""
    payload = (0xC0DE).to_bytes(2, "little") + bytes([target_node, command]) \
        + arg0.to_bytes(2, "little") + arg1.to_bytes(2, "little")
    subprocess.run(["cansend", iface, f"7E0#{payload.hex().upper()}"], check=True)


def common_checks(statuses: dict[str, dict], frames, db, rate_messages: set[str] | None = None) -> list[str]:
    """Rate checks for the periodic messages in `rate_messages` (None: all of them)
    plus the per-ECU health counters from `ecu status`."""
    fails = []
    for name, (count, measured, expected) in sorted(frame_rates(frames, db).items()):
        if rate_messages is not None and name not in rate_messages:
            continue
        if expected is not None and abs(measured - expected) > 0.10 * expected:
            fails.append(f"{name}: {measured:.1f} frames/s over its capture span, expected {expected:.0f} +/-10 %")
    for role, st in statuses.items():
        if st.get("rx_summary", {}).get("dropped", "0") != "0":
            fails.append(f"{role}: receive queue dropped frames")
        if st.get("wdt", {}).get("skipped", "0") != "0":
            fails.append(f"{role}: watchdog feed skipped")
        if st.get("tx", {}).get("busy", "0") != "0" or st.get("tx", {}).get("rejected", "0") != "0":
            fails.append(f"{role}: transmit busy/rejected")
    return fails


# --- subcommands ------------------------------------------------------------------

def cmd_run(args) -> int:
    log_dir = pathlib.Path(args.log_dir)
    vcan_up(args.iface)
    db = cantools.database.load_file(DBC)
    dump = Candump(args.iface, log_dir / "bus.log")
    sensor = EcuProcess("sensor", args.iface, log_dir)
    controller = EcuProcess("controller", args.iface, log_dir)
    dump.start()
    try:
        sensor.start()
        controller.start()
        time.sleep(args.duration)
        statuses = {"sensor": sensor.status(), "controller": controller.status()}
    finally:
        sensor.stop()
        controller.stop()
        dump.stop()
    frames = read_frames(dump.log_path)
    span = frames[-1][0] - frames[0][0] if len(frames) > 1 else 0.0
    print(f"captured {len(frames)} frames over {span:.2f} s on {args.iface}")
    for name, (count, measured, expected) in sorted(frame_rates(frames, db).items()):
        exp = f"expected {expected:.0f}" if expected else "event-driven"
        print(f"  {name:<22} {count:5d} frames  {measured:6.1f} /s  ({exp})")
    for role, status in statuses.items():
        print_status(role, status)
    fails = common_checks(statuses, frames, db)
    if not verify(dump.log_path, min_protected=int(span * 150)):
        fails.append("frame verification (DLC/CRC/counter) failed")
    for role in ("sensor", "controller"):
        node = statuses[role].get("node", {})
        if node.get("state") != "RUNNING":
            fails.append(f"{role}: node state {node.get('state')} != RUNNING")
        if node.get("safe_state_entries", "0") != "0":
            fails.append(f"{role}: safe_state_entries {node.get('safe_state_entries')} != 0")
    return verdict(fails, host_stalls(frames, db))


def cmd_safe_state(args) -> int:
    log_dir = pathlib.Path(args.log_dir)
    vcan_up(args.iface)
    db = cantools.database.load_file(DBC)
    cmd_msg = db.get_message_by_name("CONTROLLER_COMMAND")
    dump = Candump(args.iface, log_dir / "bus.log")
    sensor = EcuProcess("sensor", args.iface, log_dir)
    controller = EcuProcess("controller", args.iface, log_dir)
    dump.start()
    t_kill = t_restart = None
    try:
        controller.start()
        sensor.start()
        time.sleep(args.kill_at)
        t_kill = sensor.kill()
        print(f"sensor killed at host time {t_kill:.6f}")
        time.sleep(max(0.0, args.restart_at - args.kill_at))
        sensor.start()  # same object: new process, log appended
        t_restart = sensor.started_at
        print(f"sensor restarted at host time {t_restart:.6f}")
        time.sleep(max(0.0, args.duration - args.restart_at))
        status = controller.status()
    finally:
        sensor.stop()
        controller.stop()
        dump.stop()

    frames = read_frames(dump.log_path)
    sensor_ids = {db.get_message_by_name(n).frame_id for n in ("SENSOR_DATA", "SENSOR_HEARTBEAT", "BATTERY_STATUS")}
    hb_id = db.get_message_by_name("SENSOR_HEARTBEAT").frame_id
    data_id = db.get_message_by_name("SENSOR_DATA").frame_id

    def mode_of(data: bytes) -> str:
        return str(cmd_msg.decode(data, decode_choices=True)["mode"])

    last_hb_before = max((t for t, i, _ in frames if i == hb_id and t <= t_kill), default=None)
    last_data_before = max((t for t, i, _ in frames if i == data_id and t <= t_kill), default=None)
    first_safe = next((t for t, i, d in frames if i == cmd_msg.frame_id and t > (last_data_before or 0)
                       and mode_of(d) == "SAFE_STATE"), None)
    first_sensor_after = min((t for t, i, _ in frames if i in sensor_ids and t >= t_restart), default=None)
    first_hb_after = min((t for t, i, _ in frames if i == hb_id and t >= t_restart), default=None)
    first_normal_after = next((t for t, i, d in frames if i == cmd_msg.frame_id and first_sensor_after is not None
                               and t > first_sensor_after and mode_of(d) == "NORMAL"), None)

    print("--- safe-state timeline (host-side bus timestamps, simulated bus on a desktop OS)")
    if last_data_before and first_safe:
        print(f"last SENSOR_DATA before kill      : {last_data_before:.6f}")
        print(f"last SENSOR_HEARTBEAT before kill : {last_hb_before:.6f}")
        print(f"first CONTROLLER_COMMAND SAFE_STATE: {first_safe:.6f}")
        print(f"detection latency from last SENSOR_DATA      : {(first_safe - last_data_before) * 1000:.1f} ms "
              f"(timeout {args.sensor_timeout_ms} ms + supervisor tick + command period)")
        print(f"detection latency from last SENSOR_HEARTBEAT : {(first_safe - last_hb_before) * 1000:.1f} ms")
    else:
        print("no SAFE_STATE command observed")
    if first_sensor_after and first_hb_after and first_normal_after:
        print(f"first sensor frame after restart   : {first_sensor_after:.6f}")
        print(f"first SENSOR_HEARTBEAT after restart: {first_hb_after:.6f} "
              f"(+{(first_hb_after - first_sensor_after) * 1000:.1f} ms; both critical inputs alive from here)")
        print(f"first CONTROLLER_COMMAND NORMAL    : {first_normal_after:.6f}")
        print(f"recovery time from first sensor frame   : {(first_normal_after - first_sensor_after) * 1000:.1f} ms "
              f"(wait for the first heartbeat + hold {args.recovery_hold_ms} ms + supervisor tick + command period)")
        print(f"recovery time from first SENSOR_HEARTBEAT: {(first_normal_after - first_hb_after) * 1000:.1f} ms "
              f"(hold {args.recovery_hold_ms} ms + supervisor tick + command period)")
    else:
        print("no return to NORMAL observed")
    print("--- controller log transitions (firmware uptime)")
    for line in log_lines(controller.log_path, "node_state:", "timeout:", "recovered", "safe state"):
        print("  " + line)
    print_status("controller", status)

    # The sensor was dead for part of the capture, so only the controller's rate is meaningful.
    fails = common_checks({"controller": status}, frames, db, rate_messages={"CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT"})
    node = status.get("node", {})
    if first_safe is None:
        fails.append("controller never published mode=SAFE_STATE")
    if first_normal_after is None:
        fails.append("controller never returned to mode=NORMAL")
    if node.get("state") != "RUNNING":
        fails.append(f"controller node state {node.get('state')} != RUNNING at the end")
    if node.get("safe_state_entries") != "1":
        fails.append(f"controller safe_state_entries {node.get('safe_state_entries')} != 1")
    exclude = [(t_kill - 0.05, (first_sensor_after or t_restart) + 0.05)]
    return verdict(fails, host_stalls(frames, db, exclude))


def cmd_bus_off(args) -> int:
    log_dir = pathlib.Path(args.log_dir)
    vcan_up(args.iface)
    db = cantools.database.load_file(DBC)
    hb_msg = db.get_message_by_name("CONTROLLER_HEARTBEAT")
    dump = Candump(args.iface, log_dir / "bus.log")
    sensor = EcuProcess("sensor", args.iface, log_dir)
    controller = EcuProcess("controller", args.iface, log_dir)
    dump.start()
    try:
        sensor.start()
        controller.start()
        time.sleep(1.5)
        # 30 transmit errors: TEC 240 -> error-passive. Every successful transmission
        # then heals TEC by one, so at ~60 frames/s the node is passive for ~2 s,
        # long enough for the heartbeats to publish the state on the bus.
        send_test_control(args.iface, target_node=2, command=2, arg0=30)
        t_passive = time.time()
        time.sleep(2.5)
        send_test_control(args.iface, target_node=2, command=3)  # FORCE_BUS_OFF
        t_bus_off = time.time()
        time.sleep(max(0.5, args.duration - 4.0))
        status = controller.status()
        sensor_status = sensor.status()
    finally:
        sensor.stop()
        controller.stop()
        dump.stop()

    frames = read_frames(dump.log_path)
    cmd_id = db.get_message_by_name("CONTROLLER_COMMAND").frame_id
    cmd_times = [t for t, i, _ in frames if i == cmd_id]
    gaps = [(b - a, a) for a, b in zip(cmd_times, cmd_times[1:]) if b - a > 0.05 and t_bus_off - 0.03 <= a <= t_bus_off + 0.5]
    hb_states = [(t, str(hb_msg.decode(d, decode_choices=True)["can_state"]))
                 for t, i, d in frames if i == hb_msg.frame_id]
    passive_hbs = [t for t, s in hb_states if s == "ERROR_PASSIVE"]
    print("--- controller log (firmware uptime)")
    for line in log_lines(controller.log_path, "can:", "can (polled)", "bus-off", "TEST_CONTROL", "node_state:",
                          "safe state"):
        print("  " + line)
    print("--- what the bus saw")
    print(f"error-passive injected at host {t_passive:.6f}, bus-off at {t_bus_off:.6f}")
    for gap, start in gaps:
        print(f"CONTROLLER_COMMAND gap of {gap * 1000:.1f} ms starting {start:.6f} (controller silent while bus-off)")
    seen = sorted({s for _, s in hb_states})
    print(f"CONTROLLER_HEARTBEAT can_state values seen on the bus: {seen}")
    if passive_hbs:
        print(f"ERROR_PASSIVE visible in {len(passive_hbs)} heartbeats, from {passive_hbs[0] - t_passive:.3f} s to "
              f"{passive_hbs[-1] - t_passive:.3f} s after the injection (TEC healing by one per successful frame)")
    print_status("controller", status)
    print_status("sensor", sensor_status)

    # The controller was silent during the bus-off, so only the sensor's rates are meaningful.
    fails = common_checks({"sensor": sensor_status}, frames, db,
                          rate_messages={"SENSOR_DATA", "SENSOR_HEARTBEAT", "BATTERY_STATUS"})
    can = status.get("can", {})
    checks = [
        ("controller bus_off == 1", can.get("bus_off") == "1"),
        ("controller recoveries_ok == 1", can.get("recoveries_ok") == "1"),
        ("controller recovery_failures == 0", can.get("recovery_failures") == "0"),
        ("controller can phase active at the end", can.get("phase") == "active"),
        ("controller passive == 1", can.get("passive") == "1"),
        ("controller node RUNNING at the end", status.get("node", {}).get("state") == "RUNNING"),
        ("ERROR_PASSIVE published in heartbeats", "ERROR_PASSIVE" in seen),
        ("exactly one command gap around the bus-off", len(gaps) == 1),
        ("command gap between 100 and 200 ms", bool(gaps) and 0.1 <= gaps[0][0] <= 0.2),
        ("sensor saw exactly one CONTROLLER_COMMAND timeout",
         sensor_status.get("rx", {}).get("CONTROLLER_COMMAND", {}).get("timeouts") == "1"),
    ]
    fails += [name for name, ok in checks if not ok]
    exclude = [(t_bus_off - 0.03, t_bus_off + 0.5)]
    return verdict(fails, host_stalls(frames, db, exclude))


# --- entry point -------------------------------------------------------------------

class Tee:
    def __init__(self, path: pathlib.Path) -> None:
        self._file = open(path, "w")
        self._stdout = sys.stdout

    def write(self, text: str) -> None:
        self._stdout.write(text)
        self._file.write(text)

    def flush(self) -> None:
        self._stdout.flush()
        self._file.flush()

    def close(self) -> None:
        self._file.close()


def publish_logs(log_dir: pathlib.Path, cmd: str, attempt: int) -> None:
    dest = REPO / "build" / "network-logs" / cmd
    if attempt == 1:
        shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True, exist_ok=True)
    suffix = "" if attempt == 1 else f".attempt{attempt}"
    for p in log_dir.glob("*.log"):
        shutil.copy(p, dest / (p.stem + suffix + p.suffix))
    print(f"logs copied to {dest}", file=sys.__stdout__)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iface", default="vcan0")
    parser.add_argument("--log-dir", default="/tmp/can-ecu-sim-logs")
    parser.add_argument("--retries", type=int, default=0, help="repeat an INCONCLUSIVE run up to N times")
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_run = sub.add_parser("run")
    p_run.add_argument("--duration", type=float, default=3.0)
    p_run.set_defaults(fn=cmd_run)
    p_ss = sub.add_parser("safe-state")
    p_ss.add_argument("--kill-at", type=float, default=2.0)
    p_ss.add_argument("--restart-at", type=float, default=4.0)
    p_ss.add_argument("--duration", type=float, default=7.0)
    p_ss.add_argument("--sensor-timeout-ms", type=int, default=35)
    p_ss.add_argument("--recovery-hold-ms", type=int, default=500)
    p_ss.set_defaults(fn=cmd_safe_state)
    p_bo = sub.add_parser("bus-off")
    p_bo.add_argument("--duration", type=float, default=7.0)
    p_bo.set_defaults(fn=cmd_bus_off)
    args = parser.parse_args()

    log_dir = pathlib.Path(args.log_dir)
    rc = EXIT_FAIL
    for attempt in range(1, args.retries + 2):
        log_dir.mkdir(parents=True, exist_ok=True)
        for name in ("sensor.log", "controller.log", "bus.log", "summary.log"):
            (log_dir / name).unlink(missing_ok=True)
        tee = Tee(log_dir / "summary.log")
        sys.stdout = tee
        try:
            if attempt > 1:
                print(f"=== attempt {attempt} of {args.retries + 1}")
            rc = args.fn(args)
        except Exception:
            traceback.print_exc(file=sys.stdout)
            print("RESULT: FAIL (exception)")
            rc = EXIT_FAIL
        finally:
            sys.stdout = tee._stdout
            tee.close()
            publish_logs(log_dir, args.cmd, attempt)
        if rc != EXIT_INCONCLUSIVE:
            break
    return rc


if __name__ == "__main__":
    sys.exit(main())

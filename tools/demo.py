#!/usr/bin/env python3
"""A ten-second demonstration of the network at a terminal (and the source of docs/figures/demo.gif).

    python3 tools/demo.py [--sensor-exe PATH] [--iface vcan0] [--out build/demo] [--hold-ms 600] [--color]

Both ECUs start on one vcan interface with candump recording. The bus is decoded and
verified for a second. Then the sensor is told to withhold its heartbeat for 600 ms
(TEST_CONTROL SUPPRESS_HEARTBEAT, the firmware's own test hook) and the controller's
reaction is narrated live from the capture: the last heartbeat, the SAFE_STATE command
after the 350 ms heartbeat timeout, the heartbeat's return, the NORMAL command after
the 500 ms recovery hold; then the controller's own log lines and its `ecu status`.
The capture and the ECU logs are copied to --out, where tools/plots.py draws the
fault timeline from them. Every number printed is measured on a simulated bus (Linux
vcan) on a desktop OS, not on real-time hardware. Runs inside the dev container.
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys
import time

import cantools

REPO = pathlib.Path(__file__).resolve().parents[1]
for p in (REPO / "tools", REPO / "can"):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import e2e_ref  # noqa: E402  (the Python reference of the protection scheme)
import ecu_network as net  # noqa: E402  (Phase 2: ECU processes, candump, frames)
from gen_codec import is_yes  # noqa: E402  (the DBC attribute reader of the generators)

NODE_SENSOR = 1
CMD_SUPPRESS_HEARTBEAT = 4
PERIODIC = ("SENSOR_DATA", "BATTERY_STATUS", "SENSOR_HEARTBEAT", "CONTROLLER_COMMAND", "CONTROLLER_HEARTBEAT")


class Say:
    """Narration with a little colour when wanted; every line is also plain text."""

    def __init__(self, color: bool) -> None:
        self.c = color

    def _wrap(self, code: str, text: str) -> str:
        return f"\x1b[{code}m{text}\x1b[0m" if self.c else text

    def head(self, text: str) -> None:
        print()
        print(self._wrap("1;36", text))

    def line(self, text: str = "") -> None:
        print(text)

    def ok(self, text: str) -> None:
        print(self._wrap("32", text))

    def event(self, t: float, text: str) -> None:
        print(self._wrap("1;33", f"  {t:+7.3f} s") + "  " + text)

    def dim(self, text: str) -> None:
        print(self._wrap("2", text))


def pause(seconds: float) -> None:
    sys.stdout.flush()
    time.sleep(seconds)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sensor-exe", type=pathlib.Path, default=None,
                        help="the sensor executable (default: the C++ native_sim build; rust/target/release/sensor-vcan for the Rust one)")
    parser.add_argument("--controller-exe", type=pathlib.Path, default=None)
    parser.add_argument("--iface", default="vcan0")
    parser.add_argument("--out", type=pathlib.Path, default=REPO / "build" / "demo")
    parser.add_argument("--log-dir", type=pathlib.Path, default=pathlib.Path("/tmp/can-ecu-sim-demo"))
    parser.add_argument("--hold-ms", type=int, default=600, help="how long the sensor withholds its heartbeat")
    parser.add_argument("--color", action="store_true", help="ANSI colours (on by default on a terminal)")
    parser.add_argument("--fast", action="store_true", help="no reading pauses")
    args = parser.parse_args()
    say = Say(args.color or sys.stdout.isatty())
    hold = 0.0 if args.fast else 1.0

    db = cantools.database.load_file(net.DBC)
    msgs = {m.name: m for m in db.messages}
    cmd_msg = msgs["CONTROLLER_COMMAND"]
    hb_id = msgs["SENSOR_HEARTBEAT"].frame_id
    cmd_id = cmd_msg.frame_id
    protected_ids = {m.frame_id for m in db.messages if is_yes(m, "SafetyRelevant")}

    log_dir = args.log_dir
    shutil.rmtree(log_dir, ignore_errors=True)
    log_dir.mkdir(parents=True)
    net.vcan_up(args.iface)
    dump = net.Candump(args.iface, log_dir / "bus.log")
    sensor = net.EcuProcess("sensor", args.iface, log_dir, exe=args.sensor_exe)
    controller = net.EcuProcess("controller", args.iface, log_dir, exe=args.controller_exe)

    say.head("can-ecu-sim: two ECUs on a virtual CAN bus")
    say.line(f"  sensor      {sensor.exe}")
    say.line(f"  controller  {controller.exe}")
    say.line(f"  bus         {args.iface} (Linux vcan), candump recording")
    rc = 0
    try:
        dump.start()
        t_start = time.time()
        controller.start()
        sensor.start()
        say.head("1. Boot")
        states: dict[str, float] = {}
        deadline = time.monotonic() + 8.0
        while len(states) < 2 and time.monotonic() < deadline:
            for ecu in (sensor, controller):
                if ecu.role in states:
                    continue
                try:
                    if ecu.status().get("node", {}).get("state") == "RUNNING":
                        states[ecu.role] = time.time() - t_start
                        say.ok(f"  {ecu.role:<10} RUNNING {states[ecu.role] * 1000:5.0f} ms after start (inputs established)")
                except (TimeoutError, RuntimeError):
                    pass
            time.sleep(0.05)
        if len(states) < 2:
            say.line("  an ECU did not reach RUNNING; see the logs in " + str(log_dir))
            return 1
        pause(hold)

        say.head("2. The bus, decoded and verified (one second)")
        time.sleep(1.0)
        frames = net.read_frames(dump.log_path)
        t_end = frames[-1][0]
        window = [f for f in frames if f[0] > t_end - 1.0]
        for name in PERIODIC:
            m = msgs[name]
            n = sum(1 for _, i, _ in window if i == m.frame_id)
            say.line(f"  0x{m.frame_id:03X} {name:<21} {n:4d} frames/s   (DBC: every {m.cycle_time} ms)")
        latest = {}
        for t, i, d in window:
            latest[i] = d
        sd = msgs["SENSOR_DATA"].decode(latest[msgs["SENSOR_DATA"].frame_id], decode_choices=True)
        cc = cmd_msg.decode(latest[cmd_id], decode_choices=True)
        say.line(f"  last SENSOR_DATA:        speed {sd['speed']:.2f} m/s, yaw rate {sd['yaw_rate']:.1f} deg/s, "
                 f"accel {sd['accel_x']:.2f} m/s2, status {sd['sensor_status']}")
        say.line(f"  last CONTROLLER_COMMAND: mode {cc['mode']}, torque {cc['torque_cmd']:.2f} Nm, "
                 f"actuators_enabled {cc['actuators_enabled']}, brake_hold {cc['brake_hold']}")
        checked = [(i, d) for _, i, d in window if i in protected_ids]
        bad = sum(1 for i, d in checked if e2e_ref.frame_crc(i, bytes(d)) != d[0])
        say.ok(f"  {len(checked)} protected frames in the window: CRC-8 over payload and identifier "
               f"{'correct on every frame' if bad == 0 else f'WRONG on {bad}'}")
        pause(hold)

        say.head(f"3. Fault: the sensor withholds its heartbeat for {args.hold_ms} ms (TEST_CONTROL SUPPRESS_HEARTBEAT)")
        say.dim(f"  controller: SENSOR_HEARTBEAT timeout {350} ms, recovery hold {500} ms, command period 20 ms")
        net.send_test_control(args.iface, NODE_SENSOR, CMD_SUPPRESS_HEARTBEAT, args.hold_ms)
        t_cmd = time.time()
        say.event(0.0, f"TEST_CONTROL sent to the sensor: SUPPRESS_HEARTBEAT {args.hold_ms} ms")
        last_hb = None
        gap_seen = False
        t_safe = None
        t_back = None
        t_normal = None
        end = t_cmd + args.hold_ms / 1000.0 + 1.5
        while time.time() < end and t_normal is None:
            frames = net.read_frames(dump.log_path)
            hbs = [t for t, i, _ in frames if i == hb_id and t > t_cmd - 0.3]
            now = time.time()
            if not gap_seen:
                before = [t for t in hbs if t <= t_cmd + 0.12]
                if before and now - before[-1] > 0.25 and not any(before[-1] < t for t in hbs):
                    last_hb = before[-1]
                    gap_seen = True
                    say.event(last_hb - t_cmd, "last SENSOR_HEARTBEAT before the silence")
            if gap_seen and t_safe is None:
                for t, i, d in frames:
                    if i == cmd_id and t > last_hb and str(cmd_msg.decode(d, decode_choices=True)["mode"]) == "SAFE_STATE":
                        t_safe = t
                        v = cmd_msg.decode(d, decode_choices=True)
                        say.event(t - t_cmd, f"CONTROLLER_COMMAND mode=SAFE_STATE  <- {(t - last_hb) * 1000:.0f} ms after the last "
                                              f"heartbeat (timeout 350 ms)")
                        say.line(f"             actuators_enabled={v['actuators_enabled']} brake_hold={v['brake_hold']} "
                                 f"speed_limit={v['speed_limit']:.0f} torque={v['torque_cmd']:.0f}")
                        break
            if gap_seen and t_back is None:
                after = [t for t in hbs if t > last_hb + 0.25]
                if after:
                    t_back = after[0]
                    say.event(t_back - t_cmd, f"SENSOR_HEARTBEAT again, {(t_back - last_hb) * 1000:.0f} ms after the last one")
            if t_back is not None and t_safe is not None:
                for t, i, d in frames:
                    if i == cmd_id and t > t_back and str(cmd_msg.decode(d, decode_choices=True)["mode"]) == "NORMAL":
                        t_normal = t
                        say.event(t - t_cmd, f"CONTROLLER_COMMAND mode=NORMAL  <- {(t - t_back) * 1000:.0f} ms after the heartbeat's "
                                              f"return, recovery hold 500")
                        break
            time.sleep(0.03)
        if t_normal is None:
            say.line("  (the sequence did not complete in time; see the logs)")
            rc = 1
        pause(hold)

        say.head("4. The controller's own log (firmware uptime)")
        for line in net.log_lines(controller.log_path, "timeout:", "node_state:", "safe state"):
            say.line("  " + line)
        pause(hold)

        say.head("5. ecu status on the controller")
        for line in controller.shell("ecu status").splitlines():
            if line.startswith(("node:", "rx SENSOR_HEARTBEAT:", "rx SENSOR_DATA:")):
                say.line("  " + line)
        pause(hold)
    finally:
        sensor.stop()
        controller.stop()
        dump.stop()
        args.out.mkdir(parents=True, exist_ok=True)
        for name in ("bus.log", "sensor.log", "controller.log"):
            if (log_dir / name).exists():
                shutil.copy(log_dir / name, args.out / name)
    frames = net.read_frames(args.out / "bus.log")
    out = args.out.resolve()
    shown = out.relative_to(REPO) if out.is_relative_to(REPO) else out
    say.head("Done")
    say.line(f"  {len(frames)} frames captured in {frames[-1][0] - frames[0][0]:.1f} s -> {shown}/bus.log"
             f"  (tools/plots.py draws the fault timeline from it)")
    return rc


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Record the plain-bus network with the host tools, probe it, and analyse the recording.

    python3 tools/record_and_analyze.py [--duration 8] [--probes 200] [--probe-interval 0.02]
        [--bitrate 500000] [--seed 1]  (common: --iface vcan0 --log-dir /tmp/can-ecu-sim-logs --retries N)

Starts both native_sim ECUs on one vcan interface while `python3 -m host.logger`
records the bus (bus.log, bus.csv) and `python3 -m host.decoder --mode table`
watches it; sends --probes ECHO_REQUEST frames to both ECUs with host.echo;
queries both shells; stops everything and runs host.analysis on the recording.
The report (bus load, period and jitter, echo round-trip percentiles) is printed
and written to analysis.json, next to the decoder's final table and the
logger's summary. Then it checks:

  rates       every periodic message within 10 % of its DBC rate
  periods     every periodic message's mean period within 5 % of nominal
  protection  no DLC, CRC or counter fault and nothing lost in the recording
  load        the periodic messages' bus load within 10 % of the DBC nominal
  echo        each ECU answered at least 95 % of the probes; round trip median
              under 5 ms and ECU turnaround median under 2 ms (desktop-OS bounds)
  ECUs        both RUNNING, no safe-state entry, no dropped frame, no skipped
              watchdog feed, and each ECU's echo_responses counter equal to the
              number of its responses the recording matched

Verdicts and logs follow tools/ecu_network.py: a failed criterion with a host
scheduling stall in the recording is INCONCLUSIVE (exit 3) and retried; logs
are published to build/network-logs/record-and-analyze/. Every number is from a
simulated bus on a desktop OS, not real-time hardware.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ecu_network as net  # noqa: E402

sys.path.append(str(net.REPO))
from host import analysis  # noqa: E402
from host.capture import open_bus, read_log  # noqa: E402
from host.echo import EchoProbe  # noqa: E402
from host.network import SIMULATED_BUS_NOTE, Network  # noqa: E402

ROLE_OF = {"SENSOR_ECU": "sensor", "CONTROLLER_ECU": "controller"}


def wait_running(ecus: list[net.EcuProcess], timeout_s: float = 6.0) -> None:
    deadline = time.monotonic() + timeout_s
    states: dict[str, str | None] = {}
    while time.monotonic() < deadline:
        time.sleep(0.25)
        try:
            states = {e.role: e.status().get("node", {}).get("state") for e in ecus}
        except TimeoutError:
            continue  # shell not up yet
        if all(s == "RUNNING" for s in states.values()):
            return
    raise RuntimeError(f"ECUs not RUNNING after {timeout_s} s: {states}")


def host_tool(module: str, log_dir: pathlib.Path, name: str, *args: str) -> tuple[subprocess.Popen, object]:
    out = open(log_dir / f"{name}.log", "wb")
    env = dict(os.environ, PYTHONUNBUFFERED="1")
    proc = subprocess.Popen([sys.executable, "-m", module, *args], cwd=str(net.REPO),
                            stdout=out, stderr=subprocess.STDOUT, env=env)
    return proc, out


def finish(proc: subprocess.Popen, out, sig: int | None = None, timeout_s: float = 10.0) -> int:
    if proc.poll() is None and sig is not None:
        proc.send_signal(sig)
    try:
        proc.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    out.close()
    return proc.returncode


def cmd_record(args) -> int:
    log_dir = pathlib.Path(args.log_dir)
    net.vcan_up(args.iface)
    network = Network()
    logger, logger_out = host_tool("host.logger", log_dir, "logger", "--iface", args.iface,
                                   "--out", str(log_dir), "--duration", "0")
    decoder, decoder_out = host_tool("host.decoder", log_dir, "decoder", "--iface", args.iface,
                                     "--mode", "table", "--duration", str(args.duration))
    time.sleep(0.3)  # both sockets open before the first frame
    sensor = net.EcuProcess("sensor", args.iface, log_dir)
    controller = net.EcuProcess("controller", args.iface, log_dir)
    t0 = time.monotonic()
    live_rows = []
    try:
        sensor.start()
        controller.start()
        wait_running([sensor, controller])
        time.sleep(1.0)
        bus = open_bus(args.iface, receive_own_messages=True)
        try:
            probe_frames = EchoProbe(bus, network, "ALL", seed=args.seed).run(args.probes, args.probe_interval, 0.5)
        finally:
            bus.shutdown()
        live_rows = analysis.match_echo(probe_frames, network)
        remaining = args.duration - (time.monotonic() - t0)
        if remaining > 0:
            time.sleep(remaining)
        statuses = {"sensor": sensor.status(), "controller": controller.status()}
    finally:
        sensor.stop()
        controller.stop()
        logger_rc = finish(logger, logger_out, signal.SIGINT)
        decoder_rc = finish(decoder, decoder_out)

    frames = read_log(log_dir / "bus.log")
    if not frames:
        print("the logger recorded nothing")
        print("RESULT: FAIL")
        return net.EXIT_FAIL
    report = analysis.analyze(frames, network, bitrate=args.bitrate, source="bus.log")
    (log_dir / "analysis.json").write_text(json.dumps(analysis.to_dict(report), indent=2))

    print(f"=== python3 -m host.analysis bus.log --bitrate {args.bitrate} (the recording of python3 -m host.logger)")
    print(analysis.format_report(report))
    print(f"=== host.echo as matched live by the probe itself ({args.probes} probes to ALL, "
          f"{args.probe_interval * 1000:g} ms apart; same kernel timestamps)")
    print(analysis.format_echo(live_rows))
    print(f"=== python3 -m host.decoder --mode table, final table (exit {decoder_rc})")
    print((log_dir / "decoder.log").read_text(errors="replace").rstrip())
    print(f"=== python3 -m host.logger summary (exit {logger_rc})")
    print((log_dir / "logger.log").read_text(errors="replace").rstrip())
    for role, status in statuses.items():
        net.print_status(role, status)

    tuples = [(f.timestamp, f.can_id, f.data) for f in frames]
    fails = net.common_checks(statuses, tuples, network.db)
    for p in report.periods:
        if abs(p.mean_ms - p.nominal_ms) > 0.05 * p.nominal_ms:
            fails.append(f"{p.name}: mean period {p.mean_ms:.3f} ms, nominal {p.nominal_ms:.0f} +/-5 %")
    if report.dlc_bad or report.crc_bad or report.repeated or report.out_of_sequence or report.lost:
        fails.append(f"protection faults in the recording: dlc {report.dlc_bad} crc {report.crc_bad} "
                     f"repeated {report.repeated} out-of-sequence {report.out_of_sequence} lost {report.lost}")
    if report.unknown:
        fails.append(f"unknown identifiers in the recording: {report.unknown}")
    L = report.load
    if abs(L.periodic_bits_per_s - L.nominal_bits_per_s) > 0.10 * L.nominal_bits_per_s:
        fails.append(f"periodic bus load {L.pct(L.periodic_bits_per_s):.2f} % vs nominal "
                     f"{L.pct(L.nominal_bits_per_s):.2f} % (+/-10 %)")
    echo_rows = {r.responder: r for r in report.echo}
    for node, role in ROLE_OF.items():
        r = echo_rows.get(node)
        if r is None or r.sent == 0:
            fails.append(f"{node}: no echo probes in the recording")
            continue
        if r.answered < 0.95 * r.sent:
            fails.append(f"{node}: answered {r.answered} of {r.sent} probes (< 95 %)")
        if r.rtt_ms and r.rtt(50) > 5.0:
            fails.append(f"{node}: echo round trip median {r.rtt(50):.3f} ms > 5 ms")
        if r.turnaround_ms and r.turnaround(50) > 2.0:
            fails.append(f"{node}: turnaround median {r.turnaround(50):.3f} ms > 2 ms")
        counted = statuses[role].get("rx_summary", {}).get("echo_responses")
        if counted != str(r.answered):
            fails.append(f"{role}: echo_responses={counted} in `ecu status`, {r.answered} responses matched in the recording")
    for role, status in statuses.items():
        node = status.get("node", {})
        if node.get("state") != "RUNNING":
            fails.append(f"{role}: node state {node.get('state')} != RUNNING")
        if node.get("safe_state_entries", "0") != "0":
            fails.append(f"{role}: safe_state_entries {node.get('safe_state_entries')} != 0")
    if logger_rc != 0:
        fails.append(f"host.logger exit code {logger_rc}")
    if decoder_rc != 0:
        fails.append(f"host.decoder exit code {decoder_rc}")
    print(f"(every number above: {SIMULATED_BUS_NOTE})")
    return net.verdict(fails, net.host_stalls(tuples, network.db))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    net.add_common_arguments(parser)
    parser.add_argument("--duration", type=float, default=8.0, help="seconds the ECUs run")
    parser.add_argument("--probes", type=int, default=200)
    parser.add_argument("--probe-interval", type=float, default=0.02)
    parser.add_argument("--bitrate", type=int, default=analysis.DEFAULT_BITRATE)
    parser.add_argument("--seed", type=int, default=1, help="seed of the probes' payload nonce")
    args = parser.parse_args()
    return net.execute(args, "record-and-analyze", cmd_record)


if __name__ == "__main__":
    sys.exit(main())

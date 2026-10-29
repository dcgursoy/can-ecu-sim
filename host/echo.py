#!/usr/bin/env python3
"""Round-trip latency probe: send ECHO_REQUEST frames and time the ECUs' responses.

    python3 -m host.echo [--iface vcan0] [--target ALL|SENSOR_ECU|CONTROLLER_ECU] [--count 100]
        [--interval 0.02] [--timeout 0.5] [--seed N] [--log PATH] [--json PATH]

Each probe carries an 8-bit seq and a 32-bit payload unique to this run, so a
response is matched to exactly one request even after seq wraps. The socket
receives its own transmissions (CAN_RAW_RECV_OWN_MSGS), so both ends of a round
trip are kernel timestamps of the same interface: the request as it reached the
bus and the response as the ECU's frame reached it. Each response also carries
the ECU's own receive-to-transmit turnaround in microseconds, which separates
ECU processing from everything else (kernel, CAN driver, OS scheduling of the
ECU process). Reports min / p50 / p90 / p99 / max per responding node. --log
keeps every frame the probe saw in the candump format, so host.analysis
reproduces the numbers from the recording. Exit 1 if a targeted node never
answered. On vcan the numbers describe a simulated bus on a desktop OS.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import random
import sys
import time

import can

from host import analysis
from host.capture import Frame, open_bus, usable
from host.network import DBC_PATH, Network, choice_value


class _Collector(can.Listener):
    """Keeps every usable frame; the probe's own requests are marked as transmissions
    (python-can cannot tell own frames from others' on vcan, see host.capture.Frame)."""

    def __init__(self, request_id: int) -> None:
        self.request_id = request_id
        self.frames: list[Frame] = []

    def on_message_received(self, msg: can.Message) -> None:
        if usable(msg):
            msg.is_rx = msg.arbitration_id != self.request_id
            self.frames.append(Frame.from_message(msg))

    def on_error(self, exc: Exception) -> None:
        print(f"echo: receive error {exc}", file=sys.stderr)

    def stop(self) -> None:
        pass


class EchoProbe:
    """Sends ECHO_REQUEST frames at a fixed interval and collects everything the bus shows."""

    def __init__(self, bus: can.BusABC, network: Network, target: str = "ALL", seed: int | None = None) -> None:
        self.bus = bus
        self.net = network
        self.msg = network.by_name["ECHO_REQUEST"]
        self.target = target
        self.target_value = choice_value(self.msg.get_signal_by_name("target_node"), target)
        self.nonce = random.Random(seed).getrandbits(16)

    def request(self, index: int) -> can.Message:
        payload = ((self.nonce << 16) | (index & 0xFFFF)) & 0xFFFFFFFF
        data = self.msg.encode({"target_node": self.target_value, "seq": index & 0xFF, "payload": payload})
        return can.Message(arbitration_id=self.msg.frame_id, data=data, is_extended_id=False)

    def run(self, count: int, interval_s: float, timeout_s: float) -> list[Frame]:
        """Send `count` probes `interval_s` apart, wait `timeout_s` for the last answers,
        and return every frame seen meanwhile (own requests included) in time order."""
        collector = _Collector(self.msg.frame_id)
        notifier = can.Notifier(self.bus, [collector], timeout=0.1)
        try:
            next_t = time.monotonic()
            for i in range(count):
                self.bus.send(self.request(i))
                next_t += interval_s
                delay = next_t - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
            time.sleep(timeout_s)
        finally:
            notifier.stop()
        return sorted(collector.frames, key=lambda f: f.timestamp)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iface", default="vcan0")
    parser.add_argument("--interface", default="socketcan", help="python-can interface type (default socketcan)")
    parser.add_argument("--target", default="ALL", choices=("ALL", "SENSOR_ECU", "CONTROLLER_ECU"))
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--interval", type=float, default=0.02, help="seconds between probes")
    parser.add_argument("--timeout", type=float, default=0.5, help="seconds to wait for the last answers")
    parser.add_argument("--seed", type=int, help="seed of the payload nonce (default: random)")
    parser.add_argument("--log", type=pathlib.Path, help="write every frame seen to this candump-format log")
    parser.add_argument("--json", type=pathlib.Path, help="write the per-responder results as JSON")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    args = parser.parse_args(argv)

    network = Network(args.dbc)
    bus = open_bus(args.iface, args.interface, receive_own_messages=True)
    try:
        frames = EchoProbe(bus, network, args.target, args.seed).run(args.count, args.interval, args.timeout)
    finally:
        bus.shutdown()
    rows = analysis.match_echo(frames, network)
    print(f"{args.count} probes to {args.target} on {args.iface}, {args.interval * 1000:g} ms apart; "
          f"{len(frames)} frames seen")
    print(analysis.format_echo(rows, simulated=args.iface.startswith("vcan")))
    if args.log:
        writer = can.CanutilsLogWriter(str(args.log), channel=args.iface)
        for f in frames:
            writer.on_message_received(f.to_message())
        writer.stop()
    if args.json:
        args.json.write_text(json.dumps([analysis.echo_dict(r) for r in rows], indent=2))
    targeted = [r for r in rows if r.sent]
    return 0 if targeted and all(r.answered for r in targeted) else 1


if __name__ == "__main__":
    sys.exit(main())

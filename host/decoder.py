#!/usr/bin/env python3
"""Decode frames live from an interface or from a recorded log, as a stream or a table.

    python3 -m host.decoder [--iface vcan0 | --file bus.log] [--mode stream|table]
        [--ids SENSOR_DATA,0x080] [--every N] [--duration S] [--refresh 0.5]

stream  one line per frame: timestamp, message, identifier, DLC, raw bytes, the
        time since the previous frame of the same identifier, the protection
        verdict (CRC, rolling counter) and the decoded signals with units.
        --every N shows every N-th frame of each identifier; --ids filters.
table   one row per identifier, redrawn every --refresh seconds on a terminal
        (printed once at the end otherwise): count, rate over the last second,
        mean/last/max period, protection faults and the last decoded values.

Timestamps are the kernel's receive timestamps of the interface (SO_TIMESTAMPNS),
not the moment this script got around to reading the frame. On vcan they
describe a simulated bus on a desktop OS, not real-time hardware.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import pathlib
import sys
import threading
import time

from host.capture import Frame, install_stop_handlers, open_bus, read_log, receive
from host.network import DBC_PATH, SIMULATED_BUS_NOTE, E2EChecker, Network, Verdict


@dataclasses.dataclass
class Decoded:
    frame: Frame
    name: str
    message: object                 # cantools Message or None for an unknown identifier
    verdict: Verdict
    period_ms: float | None         # since the previous frame of the same identifier
    values: dict[str, object]       # decoded signals (physical units, enumerations by name)

    def values_text(self) -> str:
        if self.message is None:
            return ""
        return Network.format_values(self.message, self.values)


@dataclasses.dataclass
class MessageStats:
    can_id: int
    name: str
    count: int = 0
    first_t: float | None = None
    last_t: float | None = None
    period_sum_s: float = 0.0
    last_period_ms: float | None = None
    max_period_ms: float = 0.0
    dlc_bad: int = 0
    crc_bad: int = 0
    repeated: int = 0
    out_of_sequence: int = 0
    lost: int = 0
    last_values: dict = dataclasses.field(default_factory=dict)
    recent: collections.deque = dataclasses.field(default_factory=collections.deque)

    @property
    def mean_period_ms(self) -> float | None:
        return self.period_sum_s / (self.count - 1) * 1000.0 if self.count > 1 else None

    def rate_hz(self, now: float, window_s: float) -> float:
        """Frames per second over the last `window_s` seconds of bus time."""
        while self.recent and self.recent[0] < now - window_s:
            self.recent.popleft()
        return len(self.recent) / window_s


class Decoder:
    """Decodes frames one at a time and keeps per-identifier statistics."""

    def __init__(self, network: Network, rate_window_s: float = 1.0) -> None:
        self.net = network
        self.checker = E2EChecker(network)
        self.stats: dict[int, MessageStats] = {}
        self.rate_window_s = rate_window_s
        self.last_t: float | None = None

    def feed(self, item) -> Decoded:
        frame = item if isinstance(item, Frame) else Frame.from_message(item)
        msg = self.net.message(frame.can_id)
        name = self.net.name(frame.can_id)
        verdict = self.checker.check(frame.can_id, frame.data)
        values = self.net.decode(msg, frame.data) if msg is not None and verdict.dlc_ok else {}
        st = self.stats.get(frame.can_id)
        if st is None:
            st = self.stats[frame.can_id] = MessageStats(frame.can_id, name)
        period_ms = None
        if st.last_t is not None:
            period_ms = (frame.timestamp - st.last_t) * 1000.0
            st.period_sum_s += frame.timestamp - st.last_t
            st.last_period_ms = period_ms
            st.max_period_ms = max(st.max_period_ms, period_ms)
        if st.first_t is None:
            st.first_t = frame.timestamp
        st.last_t = frame.timestamp
        st.count += 1
        st.recent.append(frame.timestamp)
        st.dlc_bad += 0 if verdict.dlc_ok else 1
        st.crc_bad += 1 if verdict.protected and verdict.dlc_ok and not verdict.crc_ok else 0
        st.repeated += 1 if verdict.counter_check == "repeated" else 0
        st.out_of_sequence += 1 if verdict.counter_check == "out_of_sequence" else 0
        st.lost += verdict.lost
        if values:
            st.last_values = values
        self.last_t = frame.timestamp
        return Decoded(frame, name, msg, verdict, period_ms, values)

    def table(self, now: float | None = None) -> str:
        now = self.last_t if now is None else now
        lines = [f"{'message':<24} {'id':>5} {'count':>7} {'rate/s':>7} {'period ms mean/last/max':>24} "
                 f"{'dlc':>4} {'crc':>4} {'rep':>4} {'oos':>4} {'lost':>5}  last values"]
        for st in sorted(self.stats.values(), key=lambda s: s.can_id):
            rate = st.rate_hz(now, self.rate_window_s) if now is not None else 0.0
            if st.mean_period_ms is None:
                period = f"{'-':>24}"
            else:
                period = f"{st.mean_period_ms:8.2f} /{st.last_period_ms:7.2f} /{st.max_period_ms:7.2f}"
            msg = self.net.message(st.can_id)
            values = Network.format_values(msg, st.last_values) if msg is not None and st.last_values else ""
            lines.append(f"{st.name:<24} 0x{st.can_id:03X} {st.count:>7} {rate:>7.1f} {period:>24} "
                         f"{st.dlc_bad:>4} {st.crc_bad:>4} {st.repeated:>4} {st.out_of_sequence:>4} {st.lost:>5}  {values}")
        lines.append("rate: frames per second over the last second of bus time; rep/oos: repeated / "
                     "out-of-sequence counters; lost: frames the counters say were missed")
        return "\n".join(lines)


def format_stream_line(d: Decoded) -> str:
    period = f"+{d.period_ms:7.2f} ms" if d.period_ms is not None else " " * 11
    return (f"({d.frame.timestamp:.6f}) {d.name:<22} 0x{d.frame.can_id:03X} [{d.frame.dlc}] "
            f"{d.frame.data.hex().upper():<16} {period}  {d.verdict.short():<22} {d.values_text()}")


def parse_ids(net: Network, text: str | None) -> set[int] | None:
    if not text:
        return None
    return {net.lookup(part.strip()) for part in text.split(",") if part.strip()}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--iface", default="vcan0", help="interface to read live (default vcan0)")
    source.add_argument("--file", type=pathlib.Path, help="decode a recorded log instead")
    parser.add_argument("--interface", default="socketcan", help="python-can interface type (default socketcan)")
    parser.add_argument("--mode", choices=("stream", "table"), default="stream")
    parser.add_argument("--ids", help="comma-separated message names or identifiers to show")
    parser.add_argument("--every", type=int, default=1, help="stream: show every N-th frame of each identifier")
    parser.add_argument("--duration", type=float, default=0.0, help="live: stop after S seconds (0: until Ctrl-C)")
    parser.add_argument("--refresh", type=float, default=0.5, help="table: redraw interval on a terminal")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    args = parser.parse_args(argv)

    net = Network(args.dbc)
    wanted = parse_ids(net, args.ids)
    decoder = Decoder(net)
    shown: dict[int, int] = {}

    def handle(item) -> None:
        d = decoder.feed(item)
        if wanted is not None and d.frame.can_id not in wanted:
            return
        if args.mode == "stream":
            n = shown.get(d.frame.can_id, 0)
            shown[d.frame.can_id] = n + 1
            if n % max(1, args.every) == 0:
                print(format_stream_line(d))

    interactive = args.mode == "table" and sys.stdout.isatty()
    channel = args.iface
    if args.file is not None:
        frames = read_log(args.file)
        channel = frames[0].channel if frames else ""
        for frame in frames:
            handle(frame)
    else:
        stop = threading.Event()
        restore_handlers = install_stop_handlers(stop)
        bus = open_bus(args.iface, args.interface)
        next_draw = time.monotonic() + args.refresh
        try:
            for msg in receive(bus, args.duration, stop):
                handle(msg)
                if interactive and time.monotonic() >= next_draw:
                    print("\x1b[2J\x1b[H" + decoder.table(msg.timestamp), flush=True)
                    next_draw = time.monotonic() + args.refresh
        finally:
            bus.shutdown()
            restore_handlers()
    if args.mode == "table":
        if interactive:
            print("\x1b[2J\x1b[H", end="")
        print(decoder.table())
        if channel.startswith("vcan") or args.file is not None:
            print(f"({SIMULATED_BUS_NOTE})" if channel.startswith("vcan") else f"(source: {args.file})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

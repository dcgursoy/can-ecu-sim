#!/usr/bin/env python3
"""Record a CAN interface to a candump-format log and a decoded CSV.

    python3 -m host.logger [--iface vcan0] [--out build/host-logs] [--duration S]
        [--interface socketcan] [--no-csv] [--quiet]

<out>/bus.log  one line per frame in the candump log format, written by
               python-can's CanutilsLogWriter: "(timestamp) iface id#data R".
               candump, can/verify_candump.py, tools/ecu_network.py and
               host.analysis all read it.
<out>/bus.csv  one row per frame: timestamp, channel, id, name, dlc, data, the
               protection verdict (protected, crc_ok, counter, counter_check),
               the period since the previous frame of that identifier, then one
               column per DBC signal ("MESSAGE.signal", physical units,
               enumerations by name) for spreadsheets and pandas.

Timestamps are the kernel's receive timestamps (SO_TIMESTAMPNS), taken when the
sender's frame reached the interface, so they do not depend on when this
process was scheduled. The summary at the end gives each message's frame count
and rate over the capture. Stops after --duration seconds or on Ctrl-C/SIGTERM.
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import sys
import threading
import time

import can

from host.capture import install_stop_handlers, open_bus, receive, usable
from host.decoder import Decoded, Decoder
from host.network import DBC_PATH, REPO, SIMULATED_BUS_NOTE, Network

DEFAULT_OUT = REPO / "build" / "host-logs"
FIXED_COLUMNS = ["timestamp", "channel", "id", "name", "dlc", "data",
                 "protected", "crc_ok", "counter", "counter_check", "period_ms"]


class CsvWriter:
    """Decoded frames, one row each; signal columns are sparse by design."""

    def __init__(self, path: pathlib.Path, network: Network) -> None:
        self.columns = FIXED_COLUMNS + network.signal_columns
        self.index = {name: i for i, name in enumerate(self.columns)}
        self.file = open(path, "w", newline="")
        self.writer = csv.writer(self.file)
        self.writer.writerow(self.columns)

    def write(self, d: Decoded) -> None:
        row = [""] * len(self.columns)
        v = d.verdict
        row[:len(FIXED_COLUMNS)] = [
            f"{d.frame.timestamp:.6f}", d.frame.channel, f"0x{d.frame.can_id:03X}", d.name, d.frame.dlc,
            d.frame.data.hex().upper(), int(v.protected),
            "" if v.crc_ok is None else int(v.crc_ok), "" if v.counter is None else v.counter,
            v.counter_check or "", "" if d.period_ms is None else f"{d.period_ms:.3f}"]
        for signal, value in d.values.items():
            column = self.index.get(f"{d.name}.{signal}")
            if column is not None:
                row[column] = value
        self.writer.writerow(row)

    def close(self) -> None:
        self.file.close()


class Recorder:
    """Writes every usable frame to bus.log (python-can) and bus.csv (decoded)."""

    def __init__(self, network: Network, out_dir: pathlib.Path, channel: str, write_csv: bool = True) -> None:
        self.net = network
        self.out_dir = pathlib.Path(out_dir)
        self.channel = channel
        self.write_csv = write_csv
        self.decoder = Decoder(network)
        self.log_path = self.out_dir / "bus.log"
        self.csv_path = self.out_dir / "bus.csv"
        self._log: can.CanutilsLogWriter | None = None
        self._csv: CsvWriter | None = None
        self.frames = 0

    def start(self) -> None:
        self.out_dir.mkdir(parents=True, exist_ok=True)
        self._log = can.CanutilsLogWriter(str(self.log_path), channel=self.channel)
        if self.write_csv:
            self._csv = CsvWriter(self.csv_path, self.net)

    def record(self, msg: can.Message) -> Decoded | None:
        if not usable(msg) or self._log is None:
            return None
        # The logger only receives, so every line gets the "R" direction marker. python-can
        # derives is_rx from the kernel's MSG_DONTROUTE flag, which is set for every frame
        # that originated on the local host, i.e. for all traffic on a vcan interface.
        msg.is_rx = True
        self._log.on_message_received(msg)
        d = self.decoder.feed(msg)
        if self._csv is not None:
            self._csv.write(d)
        self.frames += 1
        return d

    def stop(self) -> None:
        if self._log is not None:
            self._log.stop()
            self._log = None
        if self._csv is not None:
            self._csv.close()
            self._csv = None

    def summary(self) -> str:
        stats = sorted(self.decoder.stats.values(), key=lambda s: s.can_id)
        if not stats:
            return "no frames captured"
        first = min(s.first_t for s in stats)
        last = max(s.last_t for s in stats)
        lines = [f"captured {self.frames} frames over {last - first:.3f} s on {self.channel}: "
                 f"{self.log_path}" + (f", {self.csv_path}" if self.write_csv else "")]
        for s in stats:
            span = (s.last_t - s.first_t) if s.count > 1 else 0.0
            rate = (s.count - 1) / span if span > 0 else 0.0
            nominal = self.net.cycle_ms(s.can_id)
            expect = f"nominal {1000.0 / nominal:.0f}/s" if nominal else "event-driven"
            faults = ""
            if s.dlc_bad or s.crc_bad or s.repeated or s.out_of_sequence or s.lost:
                faults = (f"  faults: dlc {s.dlc_bad}, crc {s.crc_bad}, repeated {s.repeated}, "
                          f"out-of-sequence {s.out_of_sequence}, lost {s.lost}")
            lines.append(f"  {s.name:<22} 0x{s.can_id:03X} {s.count:7d} frames {rate:7.1f}/s ({expect}){faults}")
        if self.channel.startswith("vcan"):
            lines.append(f"({SIMULATED_BUS_NOTE})")
        return "\n".join(lines)


def run(bus: can.BusABC, recorder: Recorder, duration_s: float, stop: threading.Event) -> int:
    """Record until the duration elapses or `stop` is set; returns the frame count."""
    recorder.start()
    try:
        for msg in receive(bus, duration_s, stop):
            recorder.record(msg)
    finally:
        recorder.stop()
    return recorder.frames


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iface", default="vcan0")
    parser.add_argument("--interface", default="socketcan", help="python-can interface type (default socketcan)")
    parser.add_argument("--out", type=pathlib.Path, default=DEFAULT_OUT, help=f"output directory (default {DEFAULT_OUT})")
    parser.add_argument("--duration", type=float, default=0.0, help="stop after S seconds (0: until Ctrl-C)")
    parser.add_argument("--no-csv", action="store_true", help="write only bus.log")
    parser.add_argument("--quiet", action="store_true", help="no summary")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    args = parser.parse_args(argv)

    net = Network(args.dbc)
    stop = threading.Event()
    restore_handlers = install_stop_handlers(stop)
    bus = open_bus(args.iface, args.interface)
    recorder = Recorder(net, args.out, args.iface, write_csv=not args.no_csv)
    started = time.monotonic()
    try:
        run(bus, recorder, args.duration, stop)
    finally:
        bus.shutdown()
        restore_handlers()
    if not args.quiet:
        print(recorder.summary())
        print(f"recording time {time.monotonic() - started:.1f} s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

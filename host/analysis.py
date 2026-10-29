#!/usr/bin/env python3
"""Analyse a recorded capture: bus load, period and jitter per message, echo round-trip percentiles.

    python3 -m host.analysis bus.log [--bitrate 500000] [--json PATH] [--stall-factor 3] [--dbc PATH]

bus load       the bits the captured traffic would occupy on a CAN 2.0A bus at
               --bitrate, per message and in total, without stuff bits and with
               worst-case bit stuffing (the bound schedulability analysis uses),
               next to the nominal load the DBC cycle times predict, and the
               busiest 100 ms window. vcan has no bit rate: this is a property of
               the traffic, not a measurement of the virtual interface.
period/jitter  for every message with a DBC cycle time: mean, standard deviation,
               minimum, maximum, median and 99th percentile of the time between
               consecutive frames of that identifier, the largest deviation from
               the nominal period (peak jitter) and the number of gaps longer
               than --stall-factor periods (on a desktop OS: the sending process
               was not scheduled).
echo RTT       every ECHO_REQUEST matched with the ECUs' responses by seq and
               payload: percentiles of the round trip between the two bus
               timestamps, of the ECU's own turnaround field, and of their
               difference (kernel, simulator driver and OS scheduling).

Timestamps are the kernel's. On vcan every number is measured on a simulated bus
on a desktop OS, not on real-time hardware, and the report says so.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import json
import math
import pathlib
import statistics
import sys
from typing import Sequence

from host.capture import Frame, read_log
from host.network import DBC_PATH, SIMULATED_BUS_NOTE, E2EChecker, Network

# CAN 2.0A data frame with an 11-bit identifier, without stuff bits: SOF 1, identifier 11,
# RTR 1, IDE 1, r0 1, DLC 4, CRC 15, CRC delimiter 1, ACK slot 1, ACK delimiter 1, EOF 7,
# interframe space 3 = 47 bits plus the data.
STANDARD_FRAME_OVERHEAD_BITS = 47
# Bit stuffing applies from SOF to the end of the CRC sequence: 34 bits plus the data.
STUFFED_REGION_BITS = 34
PEAK_WINDOW_S = 0.1
DEFAULT_BITRATE = 500_000


def frame_bits(dlc: int, worst_case: bool = False) -> int:
    """Bits one standard data frame occupies on the wire, interframe space included.
    `worst_case` adds the maximum number of stuff bits, floor((34 + 8*dlc - 1) / 4),
    the bound used in CAN response-time analysis; real traffic lies in between."""
    bits = STANDARD_FRAME_OVERHEAD_BITS + 8 * dlc
    if worst_case:
        bits += (STUFFED_REGION_BITS + 8 * dlc - 1) // 4
    return bits


def percentile(values: Sequence[float], p: float) -> float:
    """The p-th percentile, linearly interpolated between the closest ranks (numpy's default)."""
    if not values:
        raise ValueError("percentile of an empty sequence")
    s = sorted(values)
    k = (len(s) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    if lo == hi:
        return float(s[lo])
    return float(s[lo] + (s[hi] - s[lo]) * (k - lo))


def mean(values: Sequence[float]) -> float:
    return sum(values) / len(values)


def stdev(values: Sequence[float]) -> float:
    return statistics.stdev(values) if len(values) > 1 else 0.0


@dataclasses.dataclass
class LoadRow:
    name: str
    can_id: int
    frames: int
    rate_hz: float
    dlc: int
    bits_per_s: float
    bits_per_s_worst: float
    share_pct: float            # of the measured total, without stuff bits


@dataclasses.dataclass
class Load:
    bitrate: int
    span_s: float
    rows: list[LoadRow]
    bits_per_s: float
    bits_per_s_worst: float
    periodic_bits_per_s: float          # measured, messages with a DBC cycle time only
    nominal_bits_per_s: float           # what the DBC cycle times predict for those messages
    nominal_bits_per_s_worst: float
    peak_bits_per_s: float              # busiest PEAK_WINDOW_S window, without stuff bits

    def pct(self, bits_per_s: float) -> float:
        return 100.0 * bits_per_s / self.bitrate


@dataclasses.dataclass
class PeriodRow:
    name: str
    can_id: int
    nominal_ms: float
    count: int
    rate_hz: float
    mean_ms: float
    std_ms: float
    min_ms: float
    max_ms: float
    p50_ms: float
    p99_ms: float
    peak_dev_ms: float          # max |period - nominal|
    gaps: int                   # periods longer than stall_factor x nominal


@dataclasses.dataclass
class EchoRow:
    responder: str
    sent: int = 0               # requests addressed to this responder
    answered: int = 0
    unmatched: int = 0          # responses with no pending request
    rtt_ms: list[float] = dataclasses.field(default_factory=list)
    turnaround_ms: list[float] = dataclasses.field(default_factory=list)

    def rtt(self, p: float) -> float:
        return percentile(self.rtt_ms, p)

    def turnaround(self, p: float) -> float:
        return percentile(self.turnaround_ms, p)

    def rest(self, p: float) -> float:
        """Percentile of (round trip - turnaround): everything outside the ECU's own processing."""
        return percentile([r - t for r, t in zip(self.rtt_ms, self.turnaround_ms)], p)


@dataclasses.dataclass
class Report:
    source: str
    channel: str
    frames: int
    first_t: float
    last_t: float
    unknown: dict[int, int]
    protected: int
    dlc_bad: int
    crc_bad: int
    repeated: int
    out_of_sequence: int
    lost: int
    load: Load
    periods: list[PeriodRow]
    events: dict[str, int]          # known messages without a cycle time
    echo: list[EchoRow]
    stall_factor: float

    @property
    def span_s(self) -> float:
        return self.last_t - self.first_t

    @property
    def simulated(self) -> bool:
        return self.channel.startswith("vcan")


def analyze(frames: Sequence[Frame], network: Network, bitrate: int = DEFAULT_BITRATE,
            stall_factor: float = 3.0, source: str = "") -> Report:
    frames = sorted(frames, key=lambda f: f.timestamp)
    if not frames:
        raise ValueError("the capture has no frames")
    checker = E2EChecker(network)
    times: dict[int, list[float]] = collections.defaultdict(list)
    bits: dict[int, int] = collections.defaultdict(int)
    bits_worst: dict[int, int] = collections.defaultdict(int)
    dlcs: dict[int, int] = {}
    unknown: dict[int, int] = collections.defaultdict(int)
    protected = dlc_bad = crc_bad = repeated = out_of_sequence = lost = 0
    peak = 0
    window: collections.deque[tuple[float, int]] = collections.deque()
    window_bits = 0
    for f in frames:
        times[f.can_id].append(f.timestamp)
        b = frame_bits(f.dlc)
        bits[f.can_id] += b
        bits_worst[f.can_id] += frame_bits(f.dlc, worst_case=True)
        dlcs.setdefault(f.can_id, f.dlc)
        if network.message(f.can_id) is None:
            unknown[f.can_id] += 1
        v = checker.check(f.can_id, f.data)
        protected += v.protected
        dlc_bad += not v.dlc_ok
        crc_bad += v.protected and v.dlc_ok and not v.crc_ok
        repeated += v.counter_check == "repeated"
        out_of_sequence += v.counter_check == "out_of_sequence"
        lost += v.lost
        window.append((f.timestamp, b))
        window_bits += b
        while window and window[0][0] <= f.timestamp - PEAK_WINDOW_S:
            window_bits -= window.popleft()[1]
        peak = max(peak, window_bits)

    first_t, last_t = frames[0].timestamp, frames[-1].timestamp
    span = max(last_t - first_t, 1e-9)
    total = sum(bits.values()) / span
    total_worst = sum(bits_worst.values()) / span
    rows = []
    for can_id in sorted(bits, key=lambda i: (network.message(i) is None, i)):
        rows.append(LoadRow(network.name(can_id), can_id, len(times[can_id]), len(times[can_id]) / span,
                            dlcs[can_id], bits[can_id] / span, bits_worst[can_id] / span,
                            100.0 * bits[can_id] / span / total if total else 0.0))
    periodic_ids = {m.frame_id for m in network.periodic()}
    load = Load(
        bitrate=bitrate, span_s=span, rows=rows, bits_per_s=total, bits_per_s_worst=total_worst,
        periodic_bits_per_s=sum(bits[i] for i in bits if i in periodic_ids) / span,
        nominal_bits_per_s=sum(frame_bits(m.length) * 1000.0 / m.cycle_time for m in network.periodic()),
        nominal_bits_per_s_worst=sum(frame_bits(m.length, True) * 1000.0 / m.cycle_time for m in network.periodic()),
        peak_bits_per_s=peak / PEAK_WINDOW_S)

    periods = []
    for m in network.periodic():
        ts = times.get(m.frame_id, [])
        if len(ts) < 2:
            continue
        d = [(b - a) * 1000.0 for a, b in zip(ts, ts[1:])]
        nominal = float(m.cycle_time)
        periods.append(PeriodRow(
            m.name, m.frame_id, nominal, len(ts), (len(ts) - 1) / (ts[-1] - ts[0]) if ts[-1] > ts[0] else 0.0,
            mean(d), stdev(d), min(d), max(d), percentile(d, 50), percentile(d, 99),
            max(abs(p - nominal) for p in d), sum(1 for p in d if p > stall_factor * nominal)))
    events = {network.name(i): len(ts) for i, ts in times.items()
              if network.message(i) is not None and i not in periodic_ids}
    return Report(source, frames[0].channel, len(frames), first_t, last_t, dict(unknown), protected, dlc_bad,
                  crc_bad, repeated, out_of_sequence, lost, load, periods, events,
                  match_echo(frames, network), stall_factor)


def match_echo(frames: Sequence[Frame], network: Network) -> list[EchoRow]:
    """Pair every ECHO_REQUEST with the responses it caused; one row per responding node.
    A request is matched by (seq, payload); the same key sent again supersedes the
    earlier request, whose missing answers then count against `sent`."""
    req = network.by_name.get("ECHO_REQUEST")
    if req is None:
        return []
    responders = {m.frame_id: m.senders[0] for m in network.db.messages
                  if m.name.endswith("ECHO_RESPONSE") and m.senders}
    rows = {node: EchoRow(node) for node in responders.values()}
    pending: dict[tuple[int, int], tuple[float, set[str]]] = {}
    for f in sorted(frames, key=lambda f: f.timestamp):
        if f.can_id == req.frame_id:
            values = network.decode(req, f.data)
            if not values:
                continue
            target = str(values["target_node"])
            targets = set(rows) if target == "ALL" else {target} & set(rows)
            pending[(int(values["seq"]), int(values["payload"]))] = (f.timestamp, targets)
            for node in targets:
                rows[node].sent += 1
        elif f.can_id in responders:
            node = responders[f.can_id]
            values = network.decode(network.message(f.can_id), f.data)
            if not values:
                continue
            key = (int(values["seq"]), int(values["payload"]))
            entry = pending.get(key)
            if entry is None or node not in entry[1]:
                rows[node].unmatched += 1
                continue
            t_req, targets = entry
            rows[node].answered += 1
            rows[node].rtt_ms.append((f.timestamp - t_req) * 1000.0)
            rows[node].turnaround_ms.append(float(values["turnaround"]) / 1000.0)
            targets.discard(node)
            if not targets:
                del pending[key]
    return [rows[node] for node in sorted(rows)]


def format_echo(rows: Sequence[EchoRow], simulated: bool = True) -> str:
    if not rows or not any(r.sent for r in rows):
        return "echo round trip: no ECHO_REQUEST in this capture (python3 -m host.echo sends probes)"
    lines = [f"{'responder':<16} {'sent':>5} {'answ':>5} {'unm':>4}  "
             f"{'rtt min':>8} {'p50':>7} {'p90':>7} {'p99':>7} {'max':>7}   "
             f"{'turnaround p50':>14} {'p99':>7}   {'rtt-turnaround p50':>18} {'p99':>7}"]
    for r in rows:
        if r.rtt_ms:
            lines.append(f"{r.responder:<16} {r.sent:>5} {r.answered:>5} {r.unmatched:>4}  "
                         f"{r.rtt(0):>8.3f} {r.rtt(50):>7.3f} {r.rtt(90):>7.3f} {r.rtt(99):>7.3f} {r.rtt(100):>7.3f}   "
                         f"{r.turnaround(50):>14.3f} {r.turnaround(99):>7.3f}   {r.rest(50):>18.3f} {r.rest(99):>7.3f}")
        else:
            lines.append(f"{r.responder:<16} {r.sent:>5} {r.answered:>5} {r.unmatched:>4}  no response")
    lines.append("all in ms. rtt: ECHO_REQUEST on the bus to the response on the bus (kernel timestamps); "
                 "turnaround: the ECU's own receive-to-transmit time from its response; "
                 "the difference is the kernel, the CAN driver and the OS scheduling of the ECU process")
    answered = [r for r in rows if r.turnaround_ms]
    if answered and all(t == 0.0 for r in answered for t in r.turnaround_ms):
        lines.append("turnaround is 0 in every response: the ECU's code ran in zero simulated time, as it does on "
                     "Zephyr's native_sim, so the whole round trip is host time (the field is informative on hardware)")
    if simulated:
        lines.append(f"({SIMULATED_BUS_NOTE})")
    return "\n".join(lines)


def format_report(r: Report) -> str:
    out = []
    unknown = ", ".join(f"0x{i:03X} x{n}" for i, n in sorted(r.unknown.items())) or "none"
    out.append(f"capture: {r.source or 'frames'}: {r.frames} frames over {r.span_s:.3f} s on {r.channel or '?'}; "
               f"unknown identifiers: {unknown}")
    out.append(f"protection: {r.protected} protected frames, dlc faults {r.dlc_bad}, crc faults {r.crc_bad}, "
               f"repeated counters {r.repeated}, out-of-sequence {r.out_of_sequence}, frames lost per the counters {r.lost}")
    if r.simulated:
        out.append(f"every timing number below: {SIMULATED_BUS_NOTE}")
    L = r.load
    out.append("")
    out.append(f"bus load at {L.bitrate / 1000:g} kbit/s (CAN 2.0A, 11-bit identifiers: what this traffic would "
               f"occupy on a real bus; vcan has no bit rate)")
    out.append(f"  {'message':<24} {'id':>5} {'frames':>7} {'rate/s':>7} {'dlc':>3} {'bit/s':>8} {'share':>7}  {'worst-case bit/s':>16}")
    for row in L.rows:
        out.append(f"  {row.name:<24} 0x{row.can_id:03X} {row.frames:>7} {row.rate_hz:>7.1f} {row.dlc:>3} "
                   f"{row.bits_per_s:>8.0f} {row.share_pct:>6.1f} %  {row.bits_per_s_worst:>16.0f}")
    out.append(f"  total {L.bits_per_s:.0f} bit/s = {L.pct(L.bits_per_s):.2f} % of {L.bitrate / 1000:g} kbit/s "
               f"(worst-case stuffing {L.pct(L.bits_per_s_worst):.2f} %)")
    out.append(f"  periodic messages {L.pct(L.periodic_bits_per_s):.2f} % measured vs {L.pct(L.nominal_bits_per_s):.2f} % "
               f"nominal from the DBC cycle times ({L.pct(L.nominal_bits_per_s_worst):.2f} % worst case); "
               f"busiest {PEAK_WINDOW_S * 1000:.0f} ms window {L.pct(L.peak_bits_per_s):.2f} %")
    out.append("")
    out.append(f"period and jitter in ms (consecutive frames of one identifier; gaps: periods over "
               f"{r.stall_factor:g} x nominal, a process not scheduled)")
    out.append(f"  {'message':<24} {'nominal':>7} {'n':>6} {'rate/s':>7} {'mean':>8} {'std':>6} {'min':>7} {'max':>8} "
               f"{'p50':>7} {'p99':>7} {'peak dev':>8} {'gaps':>5}")
    for p in r.periods:
        out.append(f"  {p.name:<24} {p.nominal_ms:>7.1f} {p.count:>6} {p.rate_hz:>7.1f} {p.mean_ms:>8.3f} {p.std_ms:>6.3f} "
                   f"{p.min_ms:>7.2f} {p.max_ms:>8.2f} {p.p50_ms:>7.2f} {p.p99_ms:>7.2f} {p.peak_dev_ms:>8.2f} {p.gaps:>5}")
    if r.events:
        out.append("  event-driven: " + ", ".join(f"{n} x{c}" for n, c in sorted(r.events.items())))
    out.append("")
    out.append(format_echo(r.echo, simulated=False))
    return "\n".join(out)


def echo_dict(e: EchoRow) -> dict:
    d = {"responder": e.responder, "sent": e.sent, "answered": e.answered, "unmatched": e.unmatched}
    if e.rtt_ms:
        d["rtt_ms"] = {f"p{p:g}": e.rtt(p) for p in (0, 50, 90, 99, 100)}
        d["turnaround_ms"] = {f"p{p:g}": e.turnaround(p) for p in (0, 50, 90, 99, 100)}
        d["rest_ms"] = {f"p{p:g}": e.rest(p) for p in (0, 50, 90, 99, 100)}
    return d


def to_dict(r: Report) -> dict:
    L = r.load
    return {
        "source": r.source, "channel": r.channel, "simulated_bus": r.simulated, "note": SIMULATED_BUS_NOTE,
        "frames": r.frames, "span_s": r.span_s, "first_timestamp": r.first_t, "last_timestamp": r.last_t,
        "unknown_ids": {f"0x{i:03X}": n for i, n in r.unknown.items()},
        "protection": {"protected": r.protected, "dlc_bad": r.dlc_bad, "crc_bad": r.crc_bad,
                       "repeated": r.repeated, "out_of_sequence": r.out_of_sequence, "lost": r.lost},
        "bus_load": {
            "bitrate": L.bitrate, "span_s": L.span_s,
            "load_pct": L.pct(L.bits_per_s), "load_pct_worst_case": L.pct(L.bits_per_s_worst),
            "periodic_load_pct": L.pct(L.periodic_bits_per_s), "nominal_load_pct": L.pct(L.nominal_bits_per_s),
            "nominal_load_pct_worst_case": L.pct(L.nominal_bits_per_s_worst),
            "peak_load_pct": L.pct(L.peak_bits_per_s), "peak_window_s": PEAK_WINDOW_S,
            "messages": [dataclasses.asdict(row) for row in L.rows]},
        "periods": [dataclasses.asdict(p) for p in r.periods],
        "events": r.events,
        "echo": [echo_dict(e) for e in r.echo],
        "stall_factor": r.stall_factor,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture", type=pathlib.Path, help="recorded log (candump .log from host.logger or candump -L)")
    parser.add_argument("--bitrate", type=int, default=DEFAULT_BITRATE, help="bit rate the bus load refers to")
    parser.add_argument("--stall-factor", type=float, default=3.0)
    parser.add_argument("--json", type=pathlib.Path, help="also write the report as JSON")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    args = parser.parse_args(argv)
    network = Network(args.dbc)
    frames = read_log(args.capture)
    if not frames:
        print(f"{args.capture}: no frames", file=sys.stderr)
        return 1
    report = analyze(frames, network, args.bitrate, args.stall_factor, source=str(args.capture))
    print(format_report(report))
    if args.json:
        args.json.write_text(json.dumps(to_dict(report), indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())

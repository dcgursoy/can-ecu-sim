#!/usr/bin/env python3
"""Check frames captured with candump against the DBC and the protection scheme.

    candump -L vcan0 > frames.log          # or the plain `candump [-t a|z|d] vcan0` format
    python3 can/verify_candump.py frames.log [--dbc can/network.dbc] [--min-protected N]

For every captured frame whose identifier is in the DBC: check the DLC, decode
it with cantools, and for safety-relevant messages verify the CRC with
can/e2e_ref.py and track the rolling counter per identifier. Exits 1 if any
frame has the wrong length, a wrong CRC or a counter step other than +1, if an
unknown identifier shows up, if a line could not be parsed, or if fewer than
--min-protected protected frames were actually CRC-checked. This is the
independent, cross-language check of what the firmware puts on the bus.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

import cantools
from cantools.database.errors import DecodeError

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2e_ref  # noqa: E402
from gen_codec import is_yes, load_database  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parents[1]

# "  vcan0  200   [8]  A1 02 03 04 05 06 07 08"             candump default format
# " (1700000000.123456)  vcan0  200   [8]  A1 02 ..."       candump -t a / -t z / -t d
PLAIN_RE = re.compile(
    r"^\s*(?:\(\d+\.\d+\)\s+)?\S+\s+([0-9A-Fa-f]{3,8})\s+\[(\d+)\]\s*((?:[0-9A-Fa-f]{2}\s*)*)$")
# "(1700000000.123456) vcan0 200#A102030405060708"          candump -L / -l log format
LOG_RE = re.compile(r"^\((\d+\.\d+)\)\s+\S+\s+([0-9A-Fa-f]{3,8})#([0-9A-Fa-f]*)$")


def parse_line(line: str):
    m = LOG_RE.match(line)
    if m:
        return int(m.group(2), 16), bytes.fromhex(m.group(3))
    m = PLAIN_RE.match(line)
    if m:
        return int(m.group(1), 16), bytes.fromhex(m.group(3).replace(" ", ""))
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture", type=pathlib.Path)
    parser.add_argument("--dbc", type=pathlib.Path, default=REPO / "can" / "network.dbc")
    parser.add_argument("--min-protected", type=int, default=0,
                        help="fail unless at least this many protected frames were CRC-checked")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    db = load_database(args.dbc)
    safety = {m.frame_id for m in db.messages if is_yes(m, "SafetyRelevant")}
    trackers: dict[int, e2e_ref.CounterTracker] = {}
    counts = {"frames": 0, "protected": 0, "unparsed": 0, "unknown": 0,
              "dlc_bad": 0, "crc_bad": 0, "counter_bad": 0}

    for raw_line in args.capture.read_text().splitlines():
        if not raw_line.strip():
            continue
        parsed = parse_line(raw_line)
        if parsed is None:
            counts["unparsed"] += 1
            print(f"UNPARSED line: {raw_line.rstrip()}")
            continue
        can_id, data = parsed
        counts["frames"] += 1
        try:
            msg = db.get_message_by_frame_id(can_id)
        except KeyError:
            counts["unknown"] += 1
            print(f"UNKNOWN id 0x{can_id:03X} data {data.hex()}")
            continue

        status = ""
        if len(data) != msg.length:
            counts["dlc_bad"] += 1
            status += f" DLC MISMATCH ({len(data)} != {msg.length})"
        try:
            decoded = msg.decode(data, decode_choices=True, allow_truncated=False, allow_excess=False)
            fields = ", ".join(f"{k}={v}" for k, v in decoded.items())
        except DecodeError as exc:
            fields = f"(not decodable: {exc})"

        if can_id in safety and len(data) == msg.length:
            counts["protected"] += 1
            if not e2e_ref.crc_ok(can_id, data, dlc=msg.length):
                counts["crc_bad"] += 1
                status += " CRC MISMATCH"
            step = trackers.setdefault(can_id, e2e_ref.CounterTracker()).check(e2e_ref.counter_of(data))
            if step != "ok":
                counts["counter_bad"] += 1
                status += f" COUNTER {step.upper()}"
        if not args.quiet or status:
            print(f"{msg.name:<24} 0x{can_id:03X} {data.hex()}  {fields}{status}")

    faults = counts["unparsed"] + counts["unknown"] + counts["dlc_bad"] + counts["crc_bad"] + counts["counter_bad"]
    ok = faults == 0 and counts["frames"] > 0 and counts["protected"] >= args.min_protected
    print(f"verified {counts['frames']} frames ({counts['protected']} protected, CRC-checked): "
          f"{counts['dlc_bad']} wrong DLC, {counts['crc_bad']} bad CRC, {counts['counter_bad']} counter faults, "
          f"{counts['unknown']} unknown ids, {counts['unparsed']} unparsed lines -> {'OK' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

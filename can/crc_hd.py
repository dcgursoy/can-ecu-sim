#!/usr/bin/env python3
"""Count the error patterns a CRC-8 polynomial fails to detect at a given length.

    python3 can/crc_hd.py [--bits 72] [--max-weight 4] [--poly 0x2F 0x1D ...]

A CRC is linear, so an error pattern E is undetected exactly when the CRC of E
alone is zero (initial value and final XOR play no role). The codeword is the
n data bits followed by the 8 CRC bits: an error in CRC bit k has residue 1<<k,
an error in data bit p has the residue of that single bit. The script XORs the
residues of every combination of w codeword positions, for w = 1..max-weight,
and reports how many combinations come out zero. The smallest w with a
non-zero count is the Hamming distance of the code at that length.

This project protects 72 data bits per frame (16-bit identifier + 7 payload
bytes), so the codeword is 80 bits; the numbers it printed decided the
polynomial, see DESIGN.md.
"""

from __future__ import annotations

import argparse
import itertools
from math import comb

NAMES = {0x1D: "SAE J1850 / AUTOSAR CRC8 (E2E P01)", 0x2F: "AUTOSAR CRC8H2F (E2E P02)", 0x07: "CRC-8 CCITT"}


def residues(poly: int, n: int) -> list[int]:
    """Residue of each single-bit error of the codeword: n data bits, then 8 CRC bits."""
    out = []
    for position in range(n):
        crc = 0
        for i in range(n):
            crc ^= (1 if i == position else 0) << 7
            crc = ((crc << 1) ^ poly) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
        out.append(crc)
    out.extend(1 << k for k in range(8))  # a flipped CRC bit is its own residue
    return out


def undetected(poly: int, n: int, weight: int) -> int:
    res = residues(poly, n)
    count = 0
    for combo in itertools.combinations(range(len(res)), weight):
        x = 0
        for p in combo:
            x ^= res[p]
        if x == 0:
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bits", type=int, default=72)
    parser.add_argument("--max-weight", type=int, default=4)
    parser.add_argument("--poly", type=lambda s: int(s, 0), nargs="+", default=[0x2F, 0x1D])
    args = parser.parse_args()

    codeword = args.bits + 8
    print(f"data length {args.bits} bits, codeword {codeword} bits (data + 8 CRC bits)")
    for poly in args.poly:
        hd = None
        cells = []
        for w in range(1, args.max_weight + 1):
            bad = undetected(poly, args.bits, w)
            cells.append(f"w={w}: {bad}/{comb(codeword, w)}")
            if hd is None and bad:
                hd = w
        hd_text = f"HD={hd}" if hd else f"HD>{args.max_weight}"
        print(f"poly 0x{poly:02X} {NAMES.get(poly, ''):36s} {hd_text:6s} undetected " + "  ".join(cells))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

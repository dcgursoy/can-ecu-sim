#!/usr/bin/env python3
"""Generate C++ test vectors for the protocol library from can/network.dbc.

    python3 can/gen_test_vectors.py          # write firmware/lib/protocol/tests/generated/test_vectors.hpp
    python3 can/gen_test_vectors.py --check  # exit 1 if the committed header is out of date

The vectors are produced by two independent implementations the C++ code is
then checked against:
  * cantools encodes/decodes every message (raw and scaled), which makes it
    the oracle for bit layout, sign handling and physical-unit conversion;
  * can/e2e_ref.py computes the CRC and the protected frames.

Per message: a vector with every signal at its raw minimum, one at its raw
maximum, and N_RANDOM seeded random vectors. The seed is fixed so the header is
reproducible and `--check` is meaningful.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import random
import sys

import cantools

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import e2e_ref  # noqa: E402
from gen_codec import (  # noqa: E402
    camel,
    collect_enums,
    display_path,
    enum_member,
    enum_name_of,
    is_yes,
    load_database,
    raw_bounds,
    validate,
)

REPO = pathlib.Path(__file__).resolve().parents[1]
DBC_PATH = REPO / "can" / "network.dbc"
OUT_PATH = REPO / "firmware" / "lib" / "protocol" / "tests" / "generated" / "test_vectors.hpp"

SEED = 20261025
N_RANDOM = 30
N_CRC_VECTORS = 64


def cpp_bytes(data: bytes) -> str:
    return "{" + ", ".join(f"0x{b:02X}" for b in data) + "}"


def cpp_double(value) -> str:
    return repr(float(value))


def make_expected(msg, raw: dict) -> str:
    """A lambda building the expected struct field by field (C++17 has no designated initializers)."""
    cname = camel(msg.name)
    parts = [f"[] {{ proto::{cname} m{{}};"]
    for sig in msg.signals:
        value = raw[sig.name]
        if enum_name_of(sig):
            parts.append(f" m.{sig.name} = static_cast<proto::{enum_name_of(sig)}>({value});")
        else:
            parts.append(f" m.{sig.name} = {value};")
    parts.append(" return m; }()")
    return "".join(parts)


def gen(db, dbc_sha: str) -> str:
    rng = random.Random(SEED)
    out: list[str] = []
    w = out.append
    messages = sorted(db.messages, key=lambda m: m.frame_id)

    w("// GENERATED FILE, do not edit.")
    w("// Source: can/network.dbc  (sha256 %s)" % dbc_sha[:16])
    w(f"// Generator: can/gen_test_vectors.py  (seed {SEED}, cantools {cantools.__version__})")
    w("#pragma once")
    w("")
    w("#include <array>")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w('#include "protocol/messages.hpp"')
    w("")
    w("namespace vectors {")
    w("")
    for msg in messages:
        cname = camel(msg.name)
        nsig = len(msg.signals)
        bounds = {s.name: raw_bounds(s) for s in msg.signals}
        raws = [
            {s.name: bounds[s.name][0] for s in msg.signals},
            {s.name: bounds[s.name][1] for s in msg.signals},
        ]
        for _ in range(N_RANDOM):
            raws.append({s.name: rng.randint(*bounds[s.name]) for s in msg.signals})

        w(f"/// Index of each signal of {msg.name} in the phys array (DBC order).")
        w(f"struct {cname}Idx {{")
        for i, sig in enumerate(msg.signals):
            w(f"    static constexpr std::size_t {sig.name} = {i};")
        w("};")
        w("")
        w(f"struct {cname}Vector {{")
        w(f"    proto::{cname}::Frame bytes{{}};      ///< encoded by cantools (raw values)")
        w(f"    proto::{cname} expected;            ///< the raw values that were encoded")
        w(f"    std::array<double, {nsig}> phys{{}};  ///< cantools' scaled decode of bytes")
        w("};")
        w("")
        w(f"inline const std::array<{cname}Vector, {len(raws)}> k{cname} = {{{{")
        for raw in raws:
            data = msg.encode(raw, scaling=False, padding=False, strict=True)
            if len(data) != msg.length:
                raise SystemExit(f"{msg.name}: cantools produced {len(data)} bytes, DLC is {msg.length}")
            decoded_raw = msg.decode(data, decode_choices=False, scaling=False)
            if decoded_raw != raw:
                raise SystemExit(f"{msg.name}: cantools raw round trip mismatch {decoded_raw} != {raw}")
            phys = msg.decode(data, decode_choices=False, scaling=True)
            phys_list = ", ".join(cpp_double(phys[s.name]) for s in msg.signals)
            w(f"    {{{cpp_bytes(data)}, {make_expected(msg, raw)}, {{{phys_list}}}}},")
        w("}};")
        w("")

    # --- CRC vectors over random frames ---------------------------------------
    w("/// Random (identifier, frame) pairs with the CRC computed by can/e2e_ref.py.")
    w("struct CrcVector {")
    w("    std::uint32_t id{};")
    w("    std::array<std::uint8_t, 8> bytes{};")
    w("    std::uint8_t crc{};")
    w("};")
    w("")
    w(f"inline constexpr std::array<CrcVector, {N_CRC_VECTORS}> kCrc = {{{{")
    for _ in range(N_CRC_VECTORS):
        can_id = rng.randint(0, 0x7FF)
        data = bytes(rng.randint(0, 255) for _ in range(8))
        w(f"    {{0x{can_id:03X}U, {cpp_bytes(data)}, 0x{e2e_ref.frame_crc(can_id, data):02X}}},")
    w("}};")
    w("")

    # --- protected frames for every safety-relevant message --------------------
    w("/// Plain frame (crc byte 0) + counter -> protected frame, per can/e2e_ref.py.")
    w("struct E2eVector {")
    w("    std::uint32_t id{};")
    w("    std::array<std::uint8_t, 8> plain{};")
    w("    std::uint8_t counter{};")
    w("    std::array<std::uint8_t, 8> protected_frame{};")
    w("};")
    w("")
    e2e_rows = []
    for msg in messages:
        if not is_yes(msg, "SafetyRelevant"):
            continue
        bounds = {s.name: raw_bounds(s) for s in msg.signals}
        for counter in (0, 1, 7, 15):
            raw = {s.name: rng.randint(*bounds[s.name]) for s in msg.signals}
            raw["crc"] = 0
            raw["counter"] = 0
            plain = msg.encode(raw, scaling=False, padding=False, strict=True)
            protected = e2e_ref.protect(msg.frame_id, plain, counter)
            e2e_rows.append(f"    {{0x{msg.frame_id:03X}U, {cpp_bytes(plain)}, {counter}, {cpp_bytes(protected)}}},")
    w(f"inline constexpr std::array<E2eVector, {len(e2e_rows)}> kE2e = {{{{")
    out.extend(e2e_rows)
    w("}};")
    w("")

    # --- every enumerator with its DBC label ------------------------------------
    w("/// Every value of every generated enum with its DBC label, and the width of the")
    w("/// signal carrying it (for to_string tests over the whole raw range).")
    w("template <class E>")
    w("struct EnumVector {")
    w("    E value{};")
    w("    const char* label{};")
    w("};")
    w("")
    for name, (choices, length) in collect_enums(db).items():
        w(f"struct {name}Enum {{")
        w(f"    using Type = proto::{name};")
        w(f"    static constexpr unsigned kBits = {length};")
        w(f"    static constexpr std::array<EnumVector<proto::{name}>, {len(choices)}> kNames = {{{{")
        for value, label in choices.items():
            w(f'        {{proto::{name}::{enum_member(label)}, "{label}"}},')
        w("    }};")
        w("};")
        w("")
    w("}  // namespace vectors")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    parser.add_argument("--out", type=pathlib.Path, default=OUT_PATH)
    args = parser.parse_args()

    db = load_database(args.dbc)
    validate(db)
    text = gen(db, hashlib.sha256(args.dbc.read_bytes()).hexdigest())
    shown = display_path(args.out)

    if args.check:
        current = args.out.read_text(encoding="utf-8") if args.out.exists() else ""
        if current != text:
            print(f"{shown} is out of date; run python3 can/gen_test_vectors.py", file=sys.stderr)
            return 1
        print(f"{shown} is up to date")
        return 0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {shown}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

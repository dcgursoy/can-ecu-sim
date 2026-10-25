#!/usr/bin/env python3
"""Generate the C++17 message codec header from can/network.dbc.

    python3 can/gen_codec.py          # write firmware/lib/protocol/include/protocol/messages.hpp
    python3 can/gen_codec.py --check  # exit 1 if the committed header is out of date

The generated header is committed so the repository builds without running the
generator; CI runs --check to prove the two never drift apart.

Design of the generated code:
  * one struct per message holding the *raw* integer signal values exactly as
    they travel on the wire, plus constexpr metadata (id, DLC, cycle time,
    safety-relevant and test-only flags);
  * to_frame()/from_frame() are constexpr over std::array<uint8_t, DLC>, so a
    round trip can even be checked with static_assert; from_buffer() decodes a
    received pointer/length pair;
  * physical-unit accessors (<name>_phys / set_<name>_phys) only for signals
    with a scale or offset, in float, with saturation at the raw range;
  * enum classes for signals carrying the DBC attribute "EnumName";
  * only little-endian, non-multiplexed, integer signals are supported; the
    generator refuses anything else instead of guessing.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import pathlib
import sys
from collections import OrderedDict

import cantools

REPO = pathlib.Path(__file__).resolve().parents[1]
DBC_PATH = REPO / "can" / "network.dbc"
OUT_PATH = REPO / "firmware" / "lib" / "protocol" / "include" / "protocol" / "messages.hpp"

COUNTER_SIGNAL = "counter"
CRC_SIGNAL = "crc"


def camel(name: str) -> str:
    return "".join(part.capitalize() for part in name.lower().split("_"))


def enum_member(name: str) -> str:
    return "k" + camel(name)


_DB = None  # the loaded database, for BA_DEF_DEF_ default lookups


def attr(obj, name: str, default):
    """Attribute value of a message/signal; falls back to the DBC's BA_DEF_DEF_ default."""
    attributes = obj.dbc.attributes if obj.dbc is not None and obj.dbc.attributes else {}
    value = attributes.get(name)
    if value is not None:
        return value.value
    definitions = _DB.dbc.attribute_definitions if _DB is not None and _DB.dbc is not None else {}
    definition = definitions.get(name)
    if definition is None or definition.default_value is None:
        return default
    if definition.type_name == "ENUM" and definition.choices:
        # cantools reports explicit ENUM attributes as the choice index; do the same for defaults.
        return definition.choices.index(definition.default_value)
    return definition.default_value


def is_yes(obj, name: str) -> bool:
    # ENUM attributes are exposed as the index into their choice list ("No","Yes").
    return str(attr(obj, name, 0)) in ("1", "Yes")


def cpp_int_type(length: int, signed: bool) -> str:
    width = 8 if length <= 8 else 16 if length <= 16 else 32
    return f"std::{'int' if signed else 'uint'}{width}_t"


def raw_bounds(signal) -> tuple[int, int]:
    """Raw min/max: the raw steps *inside* the DBC physical range, clamped to the bit width."""
    if signal.is_signed:
        width_min, width_max = -(1 << (signal.length - 1)), (1 << (signal.length - 1)) - 1
    else:
        width_min, width_max = 0, (1 << signal.length) - 1
    lo, hi = width_min, width_max
    if signal.minimum is not None and signal.maximum is not None:
        eps = 1e-9  # absorb binary floating point noise such as 25.5 / 0.1 = 254.99999
        lo = max(width_min, math.ceil((signal.minimum - signal.offset) / signal.scale - eps))
        hi = min(width_max, math.floor((signal.maximum - signal.offset) / signal.scale + eps))
    if lo > hi:
        raise SystemExit(f"{signal.name}: empty raw range [{lo}, {hi}]")
    return lo, hi


def validate(db) -> None:
    ids = [m.frame_id for m in db.messages]
    if len(ids) != len(set(ids)):
        raise SystemExit("duplicate frame identifiers")
    for msg in db.messages:
        if msg.is_extended_frame:
            raise SystemExit(f"{msg.name}: extended identifiers are not supported")
        if msg.length > 8:
            raise SystemExit(f"{msg.name}: classic CAN only, DLC must be <= 8")
        cycle = msg.cycle_time or 0
        if not 0 <= cycle <= 65535:
            raise SystemExit(f"{msg.name}: GenMsgCycleTime {cycle} does not fit the generated uint16")
        magic = int(attr(msg, "TestMagic", 0))
        if not 0 <= magic <= 65535:
            raise SystemExit(f"{msg.name}: TestMagic {magic} does not fit the generated uint16")
        for sig in msg.signals:
            if sig.scale <= 0:
                raise SystemExit(f"{msg.name}.{sig.name}: the factor must be positive")
            if sig.byte_order != "little_endian":
                raise SystemExit(f"{msg.name}.{sig.name}: only little-endian signals are supported")
            if sig.is_float or sig.is_multiplexer or sig.multiplexer_ids:
                raise SystemExit(f"{msg.name}.{sig.name}: float or multiplexed signals are not supported")
            if sig.length < 1 or sig.length > 32:
                raise SystemExit(f"{msg.name}.{sig.name}: signal length must be 1..32 bits")
            if sig.start + sig.length > msg.length * 8:
                raise SystemExit(f"{msg.name}.{sig.name}: signal exceeds the frame")
            if (sig.scale != 1 or sig.offset != 0) and sig.length == 32 and not sig.is_signed:
                raise SystemExit(f"{msg.name}.{sig.name}: scaled 32-bit unsigned signals are not supported "
                                 "(the physical accessors convert through int32)")
        if is_yes(msg, "SafetyRelevant"):
            if msg.length != 8:
                raise SystemExit(f"{msg.name}: safety-relevant messages are always 8 bytes (fixed E2E frame size)")
            by_name = {s.name: s for s in msg.signals}
            crc, counter = by_name.get(CRC_SIGNAL), by_name.get(COUNTER_SIGNAL)
            if crc is None or (crc.start, crc.length) != (0, 8):
                raise SystemExit(f"{msg.name}: safety-relevant messages need 'crc' at bits 0..7")
            if counter is None or (counter.start, counter.length) != (8, 4):
                raise SystemExit(f"{msg.name}: safety-relevant messages need 'counter' at bits 8..11")
        # no two signals may overlap
        used = [False] * (msg.length * 8)
        for sig in msg.signals:
            for bit in range(sig.start, sig.start + sig.length):
                if used[bit]:
                    raise SystemExit(f"{msg.name}.{sig.name}: overlaps another signal at bit {bit}")
                used[bit] = True


def collect_enums(db) -> "OrderedDict[str, tuple[OrderedDict, int]]":
    enums: OrderedDict[str, tuple[OrderedDict, int]] = OrderedDict()
    for msg in db.messages:
        for sig in msg.signals:
            name = attr(sig, "EnumName", "")
            if not name:
                continue
            if not sig.choices:
                raise SystemExit(f"{msg.name}.{sig.name}: EnumName set but no value table")
            if sig.is_signed:
                raise SystemExit(f"{msg.name}.{sig.name}: enum signals must be unsigned")
            choices = OrderedDict((int(k), str(v)) for k, v in sorted(sig.choices.items()))
            if min(choices) < 0 or max(choices) >= (1 << sig.length):
                raise SystemExit(f"{msg.name}.{sig.name}: value table entry does not fit in {sig.length} bits")
            if name in enums and enums[name] != (choices, sig.length):
                raise SystemExit(f"enum {name}: signals sharing an EnumName must have identical value tables and widths")
            enums.setdefault(name, (choices, sig.length))
    return enums


def enum_name_of(sig) -> str:
    return attr(sig, "EnumName", "") or ""


def field_type(sig) -> str:
    return enum_name_of(sig) or cpp_int_type(sig.length, sig.is_signed)


def ffloat(value: float) -> str:
    text = repr(float(value))
    return text + "F"


def gen_header(db, dbc_sha: str) -> str:
    out: list[str] = []
    w = out.append
    messages = sorted(db.messages, key=lambda m: m.frame_id)
    enums = collect_enums(db)

    w("// GENERATED FILE, do not edit.")
    w("// Source: can/network.dbc  (sha256 %s)" % dbc_sha[:16])
    w("// Generator: can/gen_codec.py  (regenerate with `python3 can/gen_codec.py`)")
    w(f"// Network version: {db.version!r}" if db.version else "// Network version: (none)")
    w("#pragma once")
    w("")
    w("#include <array>")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w('#include "protocol/codec_util.hpp"')
    w("")
    w("namespace proto {")
    w("")
    # --- enums --------------------------------------------------------------
    for name, (choices, length) in enums.items():
        underlying = cpp_int_type(length, False)
        w(f"enum class {name} : {underlying} {{")
        for value, label in choices.items():
            w(f"    {enum_member(label)} = {value},")
        w("};")
        w("")
        w(f"constexpr const char* to_string({name} value)")
        w("{")
        w("    switch (value) {")
        for value, label in choices.items():
            w(f"    case {name}::{enum_member(label)}: return \"{label}\";")
        w("    }")
        w('    return "UNKNOWN";')
        w("}")
        w("")
    # --- metadata types -----------------------------------------------------
    w("/// Static description of one signal, for diagnostics and generic tooling.")
    w("struct FieldInfo {")
    w("    const char* name;")
    w("    std::uint8_t start_bit;")
    w("    std::uint8_t length;")
    w("    bool is_signed;")
    w("    float scale;")
    w("    float offset;")
    w("    std::int64_t raw_min;")
    w("    std::int64_t raw_max;")
    w("    const char* unit;")
    w("};")
    w("")
    w("/// Static description of one message.")
    w("struct MessageInfo {")
    w("    std::uint32_t id;")
    w("    std::uint8_t dlc;")
    w("    std::uint16_t cycle_ms;  ///< 0 for event-driven messages")
    w("    bool safety_relevant;    ///< carries CRC + rolling counter")
    w("    bool test_only;          ///< accepted only by test builds")
    w("    const char* name;")
    w("};")
    w("")
    # --- messages -----------------------------------------------------------
    for msg in messages:
        cname = camel(msg.name)
        cycle = msg.cycle_time or 0
        safety = is_yes(msg, "SafetyRelevant")
        test_only = is_yes(msg, "TestOnly")
        magic = int(attr(msg, "TestMagic", 0))
        senders = [s for s in msg.senders if s != "Vector__XXX"]
        if msg.comment:
            w(f"/// {msg.comment}")
        w(f"/// Senders: {', '.join(senders) or '(none)'}")
        w(f"struct {cname} {{")
        w(f"    static constexpr std::uint32_t kId = 0x{msg.frame_id:03X}U;")
        w(f"    static constexpr std::size_t kDlc = {msg.length};")
        w(f"    static constexpr std::uint16_t kCycleMs = {cycle};")
        w(f"    static constexpr bool kSafetyRelevant = {'true' if safety else 'false'};")
        w(f"    static constexpr bool kTestOnly = {'true' if test_only else 'false'};")
        w(f'    static constexpr const char* kName = "{msg.name}";')
        if magic:
            w(f"    static constexpr std::uint16_t kMagic = 0x{magic:04X}U;")
        w("    using Frame = std::array<std::uint8_t, kDlc>;")
        w("")
        for sig in msg.signals:
            lo, hi = raw_bounds(sig)
            desc = f"bits {sig.start}..{sig.start + sig.length - 1}"
            if sig.scale != 1 or sig.offset != 0:
                desc += f", phys = raw * {sig.scale:g} + {sig.offset:g}"
            if sig.unit:
                desc += f" [{sig.unit}]"
            desc += f", raw {lo}..{hi}"
            if sig.comment:
                desc += f". {sig.comment}"
            w(f"    {field_type(sig)} {sig.name}{{}};  ///< {desc}")
        scaled = [s for s in msg.signals if (s.scale != 1 or s.offset != 0) and not enum_name_of(s)]
        if scaled:
            w("")
            w("    // Physical-unit accessors. Raw values stay the truth on the wire; the")
            w("    // setters round to the nearest raw step and saturate at the raw range.")
            for sig in scaled:
                lo, hi = raw_bounds(sig)
                ctype = cpp_int_type(sig.length, sig.is_signed)
                w(f"    constexpr float {sig.name}_phys() const")
                w("    {")
                w(f"        return codec::raw_to_phys({sig.name}, {ffloat(sig.scale)}, {ffloat(sig.offset)});")
                w("    }")
                w(f"    constexpr void set_{sig.name}_phys(float value)")
                w("    {")
                w(f"        {sig.name} = static_cast<{ctype}>(")
                w(f"            codec::phys_to_raw(value, {ffloat(sig.scale)}, {ffloat(sig.offset)}, {lo}, {hi}));")
                w("    }")
        w("")
        w("    /// Encode the raw field values into a frame.")
        w("    constexpr Frame to_frame() const")
        w("    {")
        w("        Frame d{};")
        for sig in msg.signals:
            w(f"        codec::put(d, {sig.start}, {sig.length}, static_cast<std::uint32_t>({sig.name}));")
        w("        return d;")
        w("    }")
        w("")
        w("    /// Decode a frame of exactly kDlc bytes.")
        w(f"    static constexpr {cname} from_frame(const Frame& d)")
        w("    {")
        w(f"        {cname} m{{}};")
        for sig in msg.signals:
            getter = "get_signed" if sig.is_signed else "get_unsigned"
            w(f"        m.{sig.name} = static_cast<{field_type(sig)}>(codec::{getter}(d, {sig.start}, {sig.length}));")
        w("        return m;")
        w("    }")
        w("")
        w("    /// Decode from a received buffer; false if it is shorter than kDlc.")
        w(f"    static constexpr bool from_buffer(const std::uint8_t* data, std::size_t len, {cname}& out)")
        w("    {")
        w("        if (data == nullptr || len < kDlc) {")
        w("            return false;")
        w("        }")
        w("        Frame d{};")
        w("        for (std::size_t i = 0; i < kDlc; ++i) {")
        w("            d[i] = data[i];")
        w("        }")
        w("        out = from_frame(d);")
        w("        return true;")
        w("    }")
        w("")
        w(f"    static constexpr std::array<FieldInfo, {len(msg.signals)}> kFields = {{{{")
        for sig in msg.signals:
            lo, hi = raw_bounds(sig)
            w(f'        {{"{sig.name}", {sig.start}, {sig.length}, {"true" if sig.is_signed else "false"}, '
              f'{ffloat(sig.scale)}, {ffloat(sig.offset)}, {lo}, {hi}, "{sig.unit or ""}"}},')
        w("    }};")
        w("};")
        w("")
        # operator==
        w(f"constexpr bool operator==(const {cname}& a, const {cname}& b)")
        w("{")
        comps = [f"a.{s.name} == b.{s.name}" for s in msg.signals]
        w("    return " + "\n        && ".join(comps) + ";")
        w("}")
        w(f"constexpr bool operator!=(const {cname}& a, const {cname}& b)")
        w("{")
        w("    return !(a == b);")
        w("}")
        w("")
    # --- message table ------------------------------------------------------
    w("/// All messages of the network, ordered by identifier (bus priority).")
    w(f"inline constexpr std::array<MessageInfo, {len(messages)}> kMessages = {{{{")
    for msg in messages:
        cname = camel(msg.name)
        w(f"    {{{cname}::kId, {cname}::kDlc, {cname}::kCycleMs, {cname}::kSafetyRelevant, "
          f"{cname}::kTestOnly, {cname}::kName}},")
    w("}};")
    w("")
    w("constexpr const MessageInfo* find_message(std::uint32_t id)")
    w("{")
    w("    for (const auto& m : kMessages) {")
    w("        if (m.id == id) {")
    w("            return &m;")
    w("        }")
    w("    }")
    w("    return nullptr;")
    w("}")
    w("")
    w("}  // namespace proto")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="compare with the committed header, do not write")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    parser.add_argument("--out", type=pathlib.Path, default=OUT_PATH)
    args = parser.parse_args()

    global _DB
    _DB = db = cantools.database.load_file(args.dbc, strict=True)
    validate(db)
    text = gen_header(db, hashlib.sha256(args.dbc.read_bytes()).hexdigest())
    shown = display_path(args.out)

    if args.check:
        current = args.out.read_text(encoding="utf-8") if args.out.exists() else ""
        if current != text:
            print(f"{shown} is out of date; run python3 can/gen_codec.py", file=sys.stderr)
            return 1
        print(f"{shown} is up to date")
        return 0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {shown} ({len(db.messages)} messages, {len(collect_enums(db))} enums)")
    return 0


def display_path(path: pathlib.Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO))
    except ValueError:
        return str(path)


def load_database(dbc_path: pathlib.Path):
    """Load the DBC and register it for attribute-default lookups (used by other generators)."""
    global _DB
    _DB = cantools.database.load_file(dbc_path, strict=True)
    return _DB


if __name__ == "__main__":
    sys.exit(main())

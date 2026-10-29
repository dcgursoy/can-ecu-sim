#!/usr/bin/env python3
"""Generate the Rust message codec and its conformance vectors from can/network.dbc.

    python3 can/gen_rust.py          # write rust/sensor-core/src/generated/messages.rs
                                     #   and rust/sensor-core/tests/generated/vectors.rs
    python3 can/gen_rust.py --check  # exit 1 if either committed file is out of date

The Rust codec is generated from the same DBC and the same validation rules as
the C++ one (can/gen_codec.py), and the vectors are the very same seeded
vectors the C++ unit tests are checked against (can/gen_test_vectors.py
collects them once, both emitters write them), so the two implementations are
held to one definition of the network and one set of oracle data.

Design of the generated Rust (mirrors the C++ header where Rust allows it):
  * one `struct` per message holding the raw integer signal values exactly as
    they travel on the wire, with `const` metadata (ID, DLC, cycle time,
    safety-relevant and test-only flags, name) and the `Message` trait so the
    E2E and receiver code can be generic over messages;
  * `to_frame()` / `from_frame()` over `[u8; DLC]`, both `const fn`, and
    `from_buffer(&[u8]) -> Option<Self>` for a received slice;
  * physical-unit accessors (`<name>_phys` / `set_<name>_phys`) only for
    signals with a scale or offset, in f32, with saturation at the raw range;
  * value tables become `#[repr(u8)]` enums with `from_raw` / `as_str`; the
    struct field stays the raw integer, because a Rust enum cannot hold an
    out-of-table value and a received frame may carry one (the C++ `enum
    class` keeps the value; here the typed accessor returns `None`);
  * only little-endian, non-multiplexed, integer signals, as in C++.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_codec import (  # noqa: E402
    attr,
    camel,
    collect_enums,
    display_path,
    enum_name_of,
    is_yes,
    load_database,
    raw_bounds,
    validate,
)
from gen_test_vectors import SEED, collect_vectors  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parents[1]
DBC_PATH = REPO / "can" / "network.dbc"
CODEC_PATH = REPO / "rust" / "sensor-core" / "src" / "generated" / "messages.rs"
VECTORS_PATH = REPO / "rust" / "sensor-core" / "tests" / "generated" / "vectors.rs"


def rust_int(length: int, signed: bool) -> str:
    width = 8 if length <= 8 else 16 if length <= 16 else 32
    return f"{'i' if signed else 'u'}{width}"


def snake(name: str) -> str:
    return name.lower()


def variant(label: str) -> str:
    return camel(label)


def rfloat(value: float) -> str:
    text = repr(float(value))
    if "e" in text or "E" in text:
        text = f"{float(value):.10f}".rstrip("0")
        if text.endswith("."):
            text += "0"
    return text + "_f32"


def gen_codec(db, dbc_sha: str) -> str:
    out: list[str] = []
    w = out.append
    messages = sorted(db.messages, key=lambda m: m.frame_id)
    enums = collect_enums(db)

    w("// GENERATED FILE, do not edit.")
    w("// Source: can/network.dbc  (sha256 %s)" % dbc_sha[:16])
    w("// Generator: can/gen_rust.py  (regenerate with `python3 can/gen_rust.py`)")
    w(f"// Network version: {db.version!r}" if db.version else "// Network version: (none)")
    w("")
    w("use crate::codec;")
    w("")
    # --- enums --------------------------------------------------------------
    for name, (choices, length) in enums.items():
        underlying = rust_int(length, False)
        w(f"/// Value table `{name}` ({length}-bit signal). The struct fields carry the raw")
        w("/// value; `from_raw` returns `None` for a value outside the table.")
        w("#[derive(Clone, Copy, Debug, PartialEq, Eq)]")
        w(f"#[repr({underlying})]")
        w(f"pub enum {name} {{")
        for value, label in choices.items():
            w(f"    {variant(label)} = {value},")
        w("}")
        w("")
        w(f"impl {name} {{")
        w(f"    pub const fn from_raw(raw: {underlying}) -> Option<Self> {{")
        w("        match raw {")
        for value, label in choices.items():
            w(f"            {value} => Some(Self::{variant(label)}),")
        w("            _ => None,")
        w("        }")
        w("    }")
        w("")
        w("    pub const fn as_str(self) -> &'static str {")
        w("        match self {")
        for value, label in choices.items():
            w(f'            Self::{variant(label)} => "{label}",')
        w("        }")
        w("    }")
        w("")
        w(f"    /// The DBC label of a raw value, `\"UNKNOWN\"` outside the table (the C++ `to_string`).")
        w(f"    pub const fn name_of(raw: {underlying}) -> &'static str {{")
        w("        match Self::from_raw(raw) {")
        w("            Some(v) => v.as_str(),")
        w('            None => "UNKNOWN",')
        w("        }")
        w("    }")
        w("}")
        w("")
    # --- the message trait --------------------------------------------------
    w("/// What every generated message provides; the E2E and receiver code is")
    w("/// generic over it.")
    w("pub trait Message: Copy + Default + PartialEq + core::fmt::Debug {")
    w("    const ID: u16;")
    w("    const DLC: usize;")
    w("    const CYCLE_MS: u16;")
    w("    const SAFETY_RELEVANT: bool;")
    w("    const TEST_ONLY: bool;")
    w("    const NAME: &'static str;")
    w("    /// Encode the raw field values into a frame of DLC bytes.")
    w("    fn to_frame(&self) -> [u8; 8];")
    w("    /// Decode a frame; the slice must hold at least DLC bytes.")
    w("    fn from_buffer(data: &[u8]) -> Option<Self>;")
    w("}")
    w("")
    w("/// Static description of one message (the C++ `MessageInfo`).")
    w("#[derive(Clone, Copy, Debug, PartialEq, Eq)]")
    w("pub struct MessageInfo {")
    w("    pub id: u16,")
    w("    pub dlc: u8,")
    w("    /// 0 for event-driven messages.")
    w("    pub cycle_ms: u16,")
    w("    pub safety_relevant: bool,")
    w("    pub test_only: bool,")
    w("    pub name: &'static str,")
    w("}")
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
        w("#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]")
        w(f"pub struct {cname} {{")
        for sig in msg.signals:
            lo, hi = raw_bounds(sig)
            desc = f"bits {sig.start}..{sig.start + sig.length - 1}"
            if sig.scale != 1 or sig.offset != 0:
                desc += f", phys = raw * {sig.scale:g} + {sig.offset:g}"
            if sig.unit:
                desc += f" [{sig.unit}]"
            desc += f", raw {lo}..{hi}"
            if enum_name_of(sig):
                desc += f", value table `{enum_name_of(sig)}`"
            if sig.comment:
                desc += f". {sig.comment}"
            w(f"    /// {desc}")
            w(f"    pub {snake(sig.name)}: {rust_int(sig.length, sig.is_signed)},")
        w("}")
        w("")
        w(f"impl {cname} {{")
        w(f"    pub const ID: u16 = 0x{msg.frame_id:03X};")
        w(f"    pub const DLC: usize = {msg.length};")
        w(f"    pub const CYCLE_MS: u16 = {cycle};")
        w(f"    pub const SAFETY_RELEVANT: bool = {'true' if safety else 'false'};")
        w(f"    pub const TEST_ONLY: bool = {'true' if test_only else 'false'};")
        w(f'    pub const NAME: &\'static str = "{msg.name}";')
        if magic:
            w(f"    pub const MAGIC: u16 = 0x{magic:04X};")
        w("")
        for sig in msg.signals:
            ename = enum_name_of(sig)
            if ename:
                w(f"    /// `{sig.name}` as its value table, `None` for a raw value outside it.")
                w(f"    pub const fn {snake(sig.name)}_enum(&self) -> Option<{ename}> {{")
                w(f"        {ename}::from_raw(self.{snake(sig.name)})")
                w("    }")
                w(f"    pub fn set_{snake(sig.name)}(&mut self, value: {ename}) {{")
                w(f"        self.{snake(sig.name)} = value as {rust_int(sig.length, False)};")
                w("    }")
                w("")
        scaled = [s for s in msg.signals if (s.scale != 1 or s.offset != 0) and not enum_name_of(s)]
        if scaled:
            w("    // Physical-unit accessors. Raw values stay the truth on the wire; the")
            w("    // setters round to the nearest raw step and saturate at the raw range.")
            for sig in scaled:
                lo, hi = raw_bounds(sig)
                rtype = rust_int(sig.length, sig.is_signed)
                w(f"    pub fn {snake(sig.name)}_phys(&self) -> f32 {{")
                w(f"        codec::raw_to_phys(i64::from(self.{snake(sig.name)}), {rfloat(sig.scale)}, {rfloat(sig.offset)})")
                w("    }")
                w(f"    pub fn set_{snake(sig.name)}_phys(&mut self, value: f32) {{")
                w(f"        self.{snake(sig.name)} = codec::phys_to_raw(value, {rfloat(sig.scale)}, {rfloat(sig.offset)}, {lo}, {hi}) as {rtype};")
                w("    }")
            w("")
        w("    /// Encode the raw field values into a frame of DLC bytes (the rest zero).")
        w("    pub const fn to_frame(&self) -> [u8; 8] {")
        w("        let mut d = [0u8; 8];")
        for sig in msg.signals:
            # 32-bit unsigned signals are already the codec's word; a cast would be a clippy finding.
            value = f"self.{snake(sig.name)}" if rust_int(sig.length, sig.is_signed) == "u32" else f"self.{snake(sig.name)} as u32"
            w(f"        codec::put(&mut d, {sig.start}, {sig.length}, {value});")
        w("        d")
        w("    }")
        w("")
        w("    /// Decode a frame of DLC bytes.")
        w("    pub const fn from_frame(d: &[u8; 8]) -> Self {")
        w("        Self {")
        for sig in msg.signals:
            getter = "get_signed" if sig.is_signed else "get_unsigned"
            rtype = rust_int(sig.length, sig.is_signed)
            cast = "" if rtype in ("u32", "i32") else f" as {rtype}"
            w(f"            {snake(sig.name)}: codec::{getter}(d, {sig.start}, {sig.length}){cast},")
        w("        }")
        w("    }")
        w("")
        w("    /// Decode from a received slice; `None` if it is shorter than DLC.")
        w("    pub fn from_buffer(data: &[u8]) -> Option<Self> {")
        w("        if data.len() < Self::DLC {")
        w("            return None;")
        w("        }")
        w("        let mut d = [0u8; 8];")
        w("        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);")
        w("        Some(Self::from_frame(&d))")
        w("    }")
        w("}")
        w("")
        w(f"impl Message for {cname} {{")
        w("    const ID: u16 = Self::ID;")
        w("    const DLC: usize = Self::DLC;")
        w("    const CYCLE_MS: u16 = Self::CYCLE_MS;")
        w("    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;")
        w("    const TEST_ONLY: bool = Self::TEST_ONLY;")
        w("    const NAME: &'static str = Self::NAME;")
        w("    fn to_frame(&self) -> [u8; 8] {")
        w("        Self::to_frame(self)")
        w("    }")
        w("    fn from_buffer(data: &[u8]) -> Option<Self> {")
        w("        Self::from_buffer(data)")
        w("    }")
        w("}")
        w("")
    # --- message table ------------------------------------------------------
    w("/// All messages of the network, ordered by identifier (bus priority).")
    w(f"pub const MESSAGES: [MessageInfo; {len(messages)}] = [")
    for msg in messages:
        cname = camel(msg.name)
        w(f"    MessageInfo {{ id: {cname}::ID, dlc: {cname}::DLC as u8, cycle_ms: {cname}::CYCLE_MS, "
          f"safety_relevant: {cname}::SAFETY_RELEVANT, test_only: {cname}::TEST_ONLY, name: {cname}::NAME }},")
    w("];")
    w("")
    w("pub fn find_message(id: u16) -> Option<&'static MessageInfo> {")
    w("    MESSAGES.iter().find(|m| m.id == id)")
    w("}")
    return "\n".join(out) + "\n"


def rust_bytes(data: bytes) -> str:
    return "[" + ", ".join(f"0x{b:02X}" for b in data) + "]"


def gen_vectors(db, vectors, dbc_sha: str) -> str:
    out: list[str] = []
    w = out.append
    w("// GENERATED FILE, do not edit.")
    w("// Source: can/network.dbc  (sha256 %s)" % dbc_sha[:16])
    w(f"// Generator: can/gen_rust.py  (seed {SEED}, the vectors of can/gen_test_vectors.py)")
    w("")
    w("use sensor_core::messages::*;")
    w("")
    w("/// A message encoded by cantools (raw values), the raw values that were")
    w("/// encoded, and cantools' scaled decode of the bytes in DBC signal order.")
    w("pub struct MessageVector<M: 'static> {")
    w("    pub bytes: [u8; 8],")
    w("    pub expected: M,")
    w("    pub phys: &'static [f64],")
    w("}")
    w("")
    for mv in vectors.messages:
        cname = camel(mv.message.name)
        w(f"pub const {mv.message.name}: [MessageVector<{cname}>; {len(mv.rows)}] = [")
        for data, raw, phys in mv.rows:
            fields = ", ".join(f"{snake(s.name)}: {raw[s.name]}" for s in mv.message.signals)
            physs = ", ".join(repr(float(phys[s.name])) for s in mv.message.signals)
            padded = data + bytes(8 - len(data))
            w(f"    MessageVector {{ bytes: {rust_bytes(padded)}, expected: {cname} {{ {fields} }}, phys: &[{physs}] }},")
        w("];")
        w("")
    w("/// Random (identifier, frame) pairs with the CRC computed by can/e2e_ref.py.")
    w(f"pub const CRC: [(u16, [u8; 8], u8); {len(vectors.crc)}] = [")
    for can_id, data, crc in vectors.crc:
        w(f"    (0x{can_id:03X}, {rust_bytes(data)}, 0x{crc:02X}),")
    w("];")
    w("")
    w("/// Plain frame (crc byte 0) + counter -> protected frame, per can/e2e_ref.py.")
    w(f"pub const E2E: [(u16, [u8; 8], u8, [u8; 8]); {len(vectors.e2e)}] = [")
    for can_id, plain, counter, protected in vectors.e2e:
        w(f"    (0x{can_id:03X}, {rust_bytes(plain)}, {counter}, {rust_bytes(protected)}),")
    w("];")
    w("")
    w("/// Every value of every value table with its DBC label and the signal width.")
    w("pub struct EnumVector {")
    w("    pub name: &'static str,")
    w("    pub bits: u32,")
    w("    pub values: &'static [(u8, &'static str)],")
    w("}")
    w("")
    w(f"pub const ENUMS: [EnumVector; {len(vectors.enums)}] = [")
    for name, (choices, length) in vectors.enums.items():
        values = ", ".join(f'({value}, "{label}")' for value, label in choices.items())
        w(f'    EnumVector {{ name: "{name}", bits: {length}, values: &[{values}] }},')
    w("];")
    w("")
    w("/// `name_of` of every generated enum, so the table above can be checked generically.")
    w("pub fn enum_name_of(name: &str, raw: u8) -> &'static str {")
    w("    match name {")
    for name in vectors.enums:
        w(f'        "{name}" => {name}::name_of(raw),')
    w('        _ => "?",')
    w("    }")
    w("}")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--dbc", type=pathlib.Path, default=DBC_PATH)
    args = parser.parse_args()

    db = load_database(args.dbc)
    validate(db)
    sha = hashlib.sha256(args.dbc.read_bytes()).hexdigest()
    outputs = {CODEC_PATH: gen_codec(db, sha), VECTORS_PATH: gen_vectors(db, collect_vectors(db), sha)}
    stale = 0
    for path, text in outputs.items():
        shown = display_path(path)
        if args.check:
            current = path.read_text(encoding="utf-8") if path.exists() else ""
            if current != text:
                print(f"{shown} is out of date; run python3 can/gen_rust.py", file=sys.stderr)
                stale += 1
            else:
                print(f"{shown} is up to date")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        print(f"wrote {shown}")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())

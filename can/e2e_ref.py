"""Reference implementation of the message protection scheme (Python, no deps).

This is the *independent* second implementation of what firmware/lib/protocol
does in C++: it generates the test vectors the C++ unit tests are checked
against, and later phases (gateway, host tools, pytest) use it to craft and
verify protected frames.

Scheme (see DESIGN.md, Phase 1):
  * byte 0            CRC-8H2F, the AUTOSAR CRC library's second CRC-8
                      (poly 0x2F, init 0xFF, xor-out 0xFF, no reflection;
                      check value for b"123456789" is 0xDF). Chosen over the
                      SAE J1850 polynomial 0x1D because it detects every 1-,
                      2- and 3-bit error over the 72 protected bits.
  * byte 1, bits 0-3  4-bit rolling counter, +1 per transmission, wraps 15 -> 0
  * CRC input         16-bit big-endian CAN identifier, then bytes 1..DLC-1
"""

from __future__ import annotations

CRC8_POLY = 0x2F
CRC8_INIT = 0xFF
CRC8_XOR_OUT = 0xFF

COUNTER_BITS = 4
COUNTER_MASK = (1 << COUNTER_BITS) - 1

# Counter delta classification used by the receiver (policy defined in DESIGN.md 10.2).
MAX_LOST_TOLERATED = 7  # delta 2..8 is "skipped", 9..15 is "out of sequence"


def crc8_update(data: bytes, crc: int = CRC8_INIT) -> int:
    """Bitwise CRC-8 (poly CRC8_POLY) over ``data`` starting from ``crc`` (no xor-out applied)."""
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ CRC8_POLY) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def crc8(data: bytes) -> int:
    """Complete CRC-8H2F of ``data`` (init and xor-out applied)."""
    return crc8_update(data) ^ CRC8_XOR_OUT


def frame_crc(can_id: int, data: bytes) -> int:
    """CRC of a protected frame: over the 16-bit big-endian id and bytes 1..end."""
    if not 0 <= can_id <= 0x7FF:
        raise ValueError(f"standard 11-bit identifier expected, got {can_id:#x}")
    if len(data) < 2:
        raise ValueError("a protected frame needs at least 2 bytes")
    return crc8(bytes([(can_id >> 8) & 0xFF, can_id & 0xFF]) + bytes(data[1:]))


def protect(can_id: int, data: bytes, counter: int) -> bytes:
    """Return ``data`` with the counter written into byte 1 and the CRC into byte 0."""
    frame = bytearray(data)
    frame[1] = (frame[1] & ~COUNTER_MASK & 0xFF) | (counter & COUNTER_MASK)
    frame[0] = frame_crc(can_id, frame)
    return bytes(frame)


def crc_ok(can_id: int, data: bytes, dlc: int | None = None) -> bool:
    """CRC check; with ``dlc`` given, a frame of any other length fails first.

    The CRC alone cannot detect truncation (it is computed over whatever was
    received), which is why receivers check the DLC before the CRC.
    """
    if dlc is not None and len(data) != dlc:
        return False
    return len(data) >= 2 and frame_crc(can_id, data) == data[0]


def counter_of(data: bytes) -> int:
    return data[1] & COUNTER_MASK


class CounterTracker:
    """Receiver-side counter check (policy defined in DESIGN.md 10.2).

    delta 0      -> "repeated"  (same frame again, or a stale retransmission)
    delta 1      -> "ok"
    delta 2..8   -> "skipped"   (delta-1 frames were lost)
    delta 9..15  -> "out_of_sequence" (an old frame arrived late, or a replay)
    Ok, repeated and skipped frames move the reference counter; an
    out-of-sequence frame is reported but leaves it unchanged, so one late
    frame produces exactly one fault.
    """

    def __init__(self) -> None:
        self.last: int | None = None
        self.lost_total = 0

    def check(self, counter: int) -> str:
        counter &= COUNTER_MASK
        if self.last is None:
            self.last = counter
            return "ok"
        delta = (counter - self.last) & COUNTER_MASK
        if delta == 0:
            return "repeated"
        if delta == 1:
            self.last = counter
            return "ok"
        if delta <= MAX_LOST_TOLERATED + 1:
            self.last = counter
            self.lost_total += delta - 1
            return "skipped"
        return "out_of_sequence"

    def reset(self) -> None:
        self.last = None


class CounterGenerator:
    """Sender-side counter: returns 0, 1, ..., 15, 0, ..."""

    def __init__(self, start: int = 0) -> None:
        self.value = start & COUNTER_MASK

    def next(self) -> int:
        value = self.value
        self.value = (self.value + 1) & COUNTER_MASK
        return value

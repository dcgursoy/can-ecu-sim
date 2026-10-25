"""pytest for the Python reference implementation (run: pytest can/)."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import e2e_ref  # noqa: E402

# Published check vectors for CRC8H2F (AUTOSAR CRC Library specification, table
# "CRC8H2F Check values") plus the generic "123456789" check value.
AUTOSAR_VECTORS = {
    "00 00 00 00": 0x12,
    "F2 01 83": 0xC2,
    "0F AA 00 55": 0xC6,
    "00 FF 55 11": 0x77,
    "33 22 55 AA BB CC DD EE FF": 0x11,
    "92 6B 55": 0x33,
    "FF FF FF FF": 0x6C,
}


def test_crc8_matches_autosar_check_values():
    for hexstr, expected in AUTOSAR_VECTORS.items():
        assert e2e_ref.crc8(bytes.fromhex(hexstr)) == expected, hexstr
    assert e2e_ref.crc8(b"123456789") == 0xDF


def test_frame_crc_covers_identifier():
    data = bytes([0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE])
    assert e2e_ref.frame_crc(0x100, data) != e2e_ref.frame_crc(0x101, data)


def test_protect_and_check_roundtrip():
    plain = bytes([0, 0xF0, 1, 2, 3, 4, 5, 6])
    frame = e2e_ref.protect(0x100, plain, counter=5)
    assert e2e_ref.counter_of(frame) == 5
    assert frame[1] & 0xF0 == 0xF0, "the upper nibble of byte 1 is not the counter"
    assert e2e_ref.crc_ok(0x100, frame)
    assert not e2e_ref.crc_ok(0x101, frame)
    corrupted = bytearray(frame)
    corrupted[4] ^= 0x01
    assert not e2e_ref.crc_ok(0x100, bytes(corrupted))


def test_every_single_bit_flip_is_detected():
    frame = e2e_ref.protect(0x200, bytes(8), counter=9)
    for bit in range(8, 64):
        flipped = bytearray(frame)
        flipped[bit // 8] ^= 1 << (bit % 8)
        assert not e2e_ref.crc_ok(0x200, bytes(flipped)), bit


def test_counter_tracker_policy():
    t = e2e_ref.CounterTracker()
    assert t.check(3) == "ok"  # first frame is always accepted
    assert t.check(4) == "ok"
    assert t.check(4) == "repeated"
    assert t.check(6) == "skipped" and t.lost_total == 1
    assert t.check(15) == "out_of_sequence" and t.last == 6  # reference does not follow a late frame
    assert t.check(7) == "ok"
    assert t.check(15) == "skipped" and t.lost_total == 1 + 7
    assert t.check(0) == "ok"  # wrap-around 15 -> 0
    assert t.check(15) == "out_of_sequence"  # delta 15: went backwards by one


def test_late_frame_costs_exactly_one_fault():
    t = e2e_ref.CounterTracker()
    assert [t.check(c) for c in (5, 6, 7, 2, 8, 9)] == ["ok", "ok", "ok", "out_of_sequence", "ok", "ok"]
    assert t.lost_total == 0


def test_truncation_is_caught_by_the_length_check_not_the_crc():
    # A CRC byte of 0xFF means the register ended at 0x00, so dropping trailing
    # zero bytes leaves the CRC valid: truncation must be rejected by the DLC.
    frame = bytes.fromhex("FF132C0100000000")
    assert e2e_ref.crc_ok(0x200, frame)
    assert e2e_ref.crc_ok(0x200, frame[:6])  # the CRC alone cannot see this
    assert not e2e_ref.crc_ok(0x200, frame[:6], dlc=8)
    assert e2e_ref.crc_ok(0x200, frame, dlc=8)


def test_counter_generator_wraps():
    g = e2e_ref.CounterGenerator(start=14)
    assert [g.next() for _ in range(4)] == [14, 15, 0, 1]

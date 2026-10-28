"""Unit tests of the fault engine with a fake clock (no sockets, no threads).

Run with `python3 -m pytest -q gateway/` (inside the dev container or on any
host with pytest; the engine has no Linux dependency)."""

from __future__ import annotations

import pathlib
import sys

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from gateway.faults import FaultConfig, FaultEngine, Frame  # noqa: E402

SENSOR_DATA = 0x100
HEARTBEAT = 0x200


def frame(can_id: int, counter: int) -> Frame:
    return Frame(can_id, bytes([0xAA, counter & 0x0F, 1, 2, 3, 4, 5, 6]))


def drain(engine: FaultEngine, now: float) -> list[Frame]:
    return list(engine.due(now))


def run_sequence(engine: FaultEngine, can_id: int, n: int, period: float = 0.010, start: float = 1.0):
    """Submit n frames of one id at a fixed period and collect every release with its time."""
    released: list[tuple[float, Frame]] = []
    t = start
    for i in range(n):
        engine.submit(frame(can_id, i), t)
        # advance the clock through the period in 1 ms steps so releases keep their order
        for step in range(10):
            now = t + (step + 1) * period / 10.0
            released.extend((now, f) for f in engine.due(now))
        t += period
    for k in range(200):  # flush whatever is still pending (held frames, long delays)
        now = t + k * 0.001
        released.extend((now, f) for f in engine.due(now))
    return released


def test_default_config_forwards_immediately_in_order():
    engine = FaultEngine()
    for i in range(5):
        engine.submit(frame(SENSOR_DATA, i), 1.0 + i * 0.01)
        out = drain(engine, 1.0 + i * 0.01)
        assert [f.data[1] for f in out] == [i]
    assert engine.stats.received == 5
    assert engine.stats.forwarded == 5
    assert engine.stats.dropped == engine.stats.delayed == 0
    assert engine.next_release() is None


def test_drop_probability_one_drops_everything_and_counts_per_id():
    engine = FaultEngine(FaultConfig(drop=1.0))
    for i in range(10):
        engine.submit(frame(SENSOR_DATA if i % 2 else HEARTBEAT, i), 1.0)
    assert drain(engine, 2.0) == []
    assert engine.stats.dropped == 10
    assert engine.stats.by_id[SENSOR_DATA]["dropped"] == 5
    assert engine.stats.by_id[HEARTBEAT]["dropped"] == 5


def test_id_filter_limits_faults_to_listed_ids():
    engine = FaultEngine(FaultConfig(drop=1.0, ids=frozenset({HEARTBEAT})))
    engine.submit(frame(SENSOR_DATA, 1), 1.0)
    engine.submit(frame(HEARTBEAT, 1), 1.0)
    out = drain(engine, 1.0)
    assert [f.can_id for f in out] == [SENSOR_DATA]
    assert engine.stats.dropped == 1


def test_fixed_delay_is_exact_and_released_only_when_due():
    engine = FaultEngine(FaultConfig(delay_ms=20.0))
    engine.submit(frame(SENSOR_DATA, 1), 1.000)
    assert drain(engine, 1.019) == []
    assert engine.next_release() == pytest.approx(1.020)
    assert [f.data[1] for f in drain(engine, 1.020)] == [1]
    assert engine.stats.delayed == 1
    assert engine.stats.max_latency_ms == pytest.approx(20.0)


def test_jitter_stays_within_bounds_and_never_reorders():
    engine = FaultEngine(FaultConfig(delay_ms=5.0, jitter_ms=5.0), seed=7)
    released = run_sequence(engine, SENSOR_DATA, 200, period=0.001)  # jitter wider than the gap
    counters = [f.data[1] for _, f in released]
    assert len(counters) == 200
    assert counters == [i & 0x0F for i in range(200)]  # FIFO preserved
    assert 0.0 <= engine.stats.max_latency_ms <= 10.0 + 1e-6
    assert engine.stats.dropped == engine.stats.reordered == 0


def test_jitter_larger_than_delay_is_rejected():
    with pytest.raises(ValueError):
        FaultConfig(delay_ms=1.0, jitter_ms=2.0)
    with pytest.raises(ValueError):
        FaultConfig(drop=1.5)


def test_duplicate_emits_the_frame_twice_back_to_back():
    engine = FaultEngine(FaultConfig(duplicate=1.0))
    engine.submit(frame(SENSOR_DATA, 3), 1.0)
    out = drain(engine, 1.0)
    assert [f.data[1] for f in out] == [3, 3]
    assert engine.stats.duplicated == 1
    assert engine.stats.forwarded == 2


def test_reorder_swaps_a_frame_with_its_successor_of_the_same_id():
    engine = FaultEngine(FaultConfig(reorder=1.0, ids=frozenset({SENSOR_DATA})), seed=1)
    engine.submit(frame(SENSOR_DATA, 0), 1.000)  # held (reorder=1.0)
    assert drain(engine, 1.005) == []
    engine.submit(frame(HEARTBEAT, 0), 1.006)  # other id: unaffected, goes straight through
    assert [f.can_id for f in drain(engine, 1.006)] == [HEARTBEAT]
    engine.submit(frame(SENSOR_DATA, 1), 1.010)  # successor: released first, then the held frame
    out = drain(engine, 1.010)
    assert [f.data[1] for f in out] == [1, 0]
    assert engine.stats.reordered == 1
    assert engine.stats.released_late == 0
    assert engine.stats.max_latency_ms == pytest.approx(10.0)
    # the successor itself is never held, so swaps do not chain into long holds
    engine.submit(frame(SENSOR_DATA, 2), 1.020)
    assert drain(engine, 1.020) == []  # frame 2 is held again (reorder=1.0)...
    engine.submit(frame(SENSOR_DATA, 3), 1.030)
    assert [f.data[1] for f in drain(engine, 1.030)] == [3, 2]


def test_held_frame_is_released_at_the_hold_deadline_without_a_successor():
    engine = FaultEngine(FaultConfig(reorder=1.0, reorder_hold_ms=50.0))
    engine.submit(frame(SENSOR_DATA, 0), 1.000)
    assert drain(engine, 1.049) == []
    assert [f.data[1] for f in drain(engine, 1.050)] == [0]
    assert engine.stats.released_late == 1
    assert engine.stats.max_latency_ms == pytest.approx(50.0)
    # and a later frame of that id is not confused with a successor
    engine.submit(frame(SENSOR_DATA, 1), 1.100)
    assert [f.data[1] for f in drain(engine, 1.100)] == []  # held again
    assert engine.pending() == 1


def test_dropped_successor_still_releases_the_held_frame():
    engine = FaultEngine(FaultConfig(reorder=1.0, drop=0.0))
    engine.submit(frame(SENSOR_DATA, 0), 1.000)
    engine.configure(FaultConfig(drop=1.0))
    engine.submit(frame(SENSOR_DATA, 1), 1.010)  # dropped, but it frees the held frame
    assert [f.data[1] for f in drain(engine, 1.010)] == [0]
    assert engine.stats.dropped == 1


def test_corrupt_flips_exactly_one_bit():
    engine = FaultEngine(FaultConfig(corrupt=1.0), seed=3)
    original = frame(SENSOR_DATA, 5)
    engine.submit(original, 1.0)
    (out,) = drain(engine, 1.0)
    diff = int.from_bytes(original.data, "big") ^ int.from_bytes(out.data, "big")
    assert diff != 0 and diff & (diff - 1) == 0  # exactly one bit set
    assert engine.stats.corrupted == 1


def test_corrupt_crc_only_touches_byte_zero():
    engine = FaultEngine(FaultConfig(corrupt=1.0, corrupt_crc_only=True), seed=3)
    for i in range(20):
        original = frame(SENSOR_DATA, i)
        engine.submit(original, 1.0)
        (out,) = drain(engine, 1.0)
        assert out.data[1:] == original.data[1:]
        assert out.data[0] != original.data[0]


def test_same_seed_gives_the_same_decisions():
    def decisions(seed: int) -> list[int]:
        engine = FaultEngine(FaultConfig(drop=0.3, duplicate=0.2, corrupt=0.1), seed=seed)
        return [f.data[1] for _, f in run_sequence(engine, SENSOR_DATA, 100)]

    assert decisions(42) == decisions(42)
    assert decisions(42) != decisions(43)


def test_probabilities_land_near_their_targets():
    engine = FaultEngine(FaultConfig(drop=0.10, duplicate=0.10, corrupt=0.10), seed=11)
    run_sequence(engine, SENSOR_DATA, 5000)
    s = engine.stats
    assert s.received == 5000
    assert 400 <= s.dropped <= 600
    assert 350 <= s.duplicated <= 550  # decided on the 90 % that were not dropped
    assert 350 <= s.corrupted <= 550
    assert s.forwarded == s.received - s.dropped + s.duplicated


def test_config_round_trip_through_json_dicts():
    cfg = FaultConfig.from_dict({"drop": 0.1, "delay_ms": 2, "ids": ["0x100", 512]})
    assert cfg.ids == frozenset({0x100, 0x200})
    assert cfg.to_dict()["ids"] == [0x100, 0x200]
    assert FaultConfig.from_dict(cfg.to_dict()) == cfg
    with pytest.raises(ValueError):
        FaultConfig.from_dict({"dorp": 0.1})
    assert not FaultConfig().active
    assert FaultConfig(delay_ms=1).active


def test_frame_validation():
    with pytest.raises(ValueError):
        Frame(0x800, b"")
    with pytest.raises(ValueError):
        Frame(0x100, bytes(9))


def test_configure_mid_stream_keeps_scheduled_frames():
    engine = FaultEngine(FaultConfig(delay_ms=10.0))
    engine.submit(frame(SENSOR_DATA, 0), 1.000)
    engine.configure(FaultConfig())
    engine.submit(frame(SENSOR_DATA, 1), 1.001)
    # FIFO: frame 1 cannot overtake frame 0, which is still in flight until 1.010
    assert drain(engine, 1.005) == []
    assert [f.data[1] for f in drain(engine, 1.010)] == [0, 1]

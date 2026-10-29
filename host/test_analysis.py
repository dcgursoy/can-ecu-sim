"""Tests of host.analysis on synthetic frames with known answers (run: pytest host/)."""

import math

import pytest

from host import analysis
from host.conftest import frame
from host.network import E2EChecker

SENSOR_DATA, CONTROLLER_COMMAND, BATTERY_STATUS = 0x100, 0x080, 0x110
SENSOR_HEARTBEAT, CONTROLLER_HEARTBEAT = 0x200, 0x201
ECHO_REQUEST, SENSOR_ECHO, CONTROLLER_ECHO = 0x300, 0x301, 0x302


def test_frame_bits_matches_the_can_2_0a_frame_format():
    # 47 bits of overhead (interframe space included) plus the data; worst-case
    # stuffing adds floor((34 + 8n - 1) / 4): the classic 135 bits for 8 data bytes.
    assert analysis.frame_bits(0) == 47
    assert analysis.frame_bits(8) == 111
    assert analysis.frame_bits(6) == 95
    assert analysis.frame_bits(0, worst_case=True) == 47 + 8
    assert analysis.frame_bits(6, worst_case=True) == 95 + 20
    assert analysis.frame_bits(8, worst_case=True) == 135


def test_percentile_interpolates_like_numpy_default():
    assert analysis.percentile([4, 1, 3, 2], 50) == 2.5
    assert analysis.percentile([4, 1, 3, 2], 0) == 1
    assert analysis.percentile([4, 1, 3, 2], 100) == 4
    assert analysis.percentile(list(range(1, 101)), 99) == pytest.approx(99.01)
    assert analysis.percentile([7.0], 99) == 7.0
    with pytest.raises(ValueError):
        analysis.percentile([], 50)


def periodic_frames(protected, name, can_id, period_s, count, t0=1000.0, **signals):
    return [frame(t0 + i * period_s, can_id, protected(name, i & 0xF, **signals)) for i in range(count)]


def test_bus_load_of_a_perfectly_periodic_message(network, protected):
    frames = periodic_frames(protected, "SENSOR_DATA", SENSOR_DATA, 0.010, 1001)  # 10.000 s span
    r = analysis.analyze(frames, network, bitrate=500_000)
    row = r.load.rows[0]
    assert row.name == "SENSOR_DATA" and row.frames == 1001 and row.dlc == 8
    assert row.rate_hz == pytest.approx(100.1, rel=1e-3)
    assert row.bits_per_s == pytest.approx(1001 * 111 / 10.0)
    assert r.load.pct(r.load.bits_per_s) == pytest.approx(100.0 * 1001 * 111 / 10.0 / 500_000)
    assert row.share_pct == pytest.approx(100.0)
    # The DBC predicts 100 + 50 + 10 + 10 + 10 frames/s of 8 bytes: 19 980 bit/s = 3.996 % of 500 kbit/s.
    assert r.load.nominal_bits_per_s == pytest.approx(180 * 111)
    assert r.load.pct(r.load.nominal_bits_per_s) == pytest.approx(3.996)
    assert r.load.nominal_bits_per_s_worst == pytest.approx(180 * 135)
    # Busiest 100 ms window: 10 or 11 frames at a 10 ms period, depending on the window edge.
    assert 10 * 111 / 0.1 <= r.load.peak_bits_per_s <= 11 * 111 / 0.1
    # A burst makes the peak stand out from the average.
    burst = frames + [frame(1005.0005 + i * 0.0001, SENSOR_DATA, protected("SENSOR_DATA", i & 0xF)) for i in range(20)]
    assert analysis.analyze(burst, network).load.peak_bits_per_s >= 30 * 111 / 0.1
    assert r.protected == 1001 and r.crc_bad == 0 and r.repeated == 0 and r.lost == 0
    assert r.unknown == {}


def test_period_statistics_and_gaps(network, protected):
    periods_ms = [10, 10, 12, 8, 10, 40, 10, 10]   # the 40 ms gap is a stall (> 3 x nominal)
    t, frames = 1000.0, []
    for i, p in enumerate([0] + periods_ms):
        t += p / 1000.0
        frames.append(frame(t, SENSOR_DATA, protected("SENSOR_DATA", i & 0xF)))
    r = analysis.analyze(frames, network)
    (p,) = r.periods
    assert p.name == "SENSOR_DATA" and p.nominal_ms == 10.0 and p.count == 9
    assert p.mean_ms == pytest.approx(sum(periods_ms) / len(periods_ms))
    assert p.min_ms == pytest.approx(8.0) and p.max_ms == pytest.approx(40.0)
    assert p.p50_ms == pytest.approx(10.0)
    assert p.peak_dev_ms == pytest.approx(30.0)
    assert p.std_ms == pytest.approx(analysis.stdev(periods_ms))
    assert p.gaps == 1
    assert p.rate_hz == pytest.approx(8 / (sum(periods_ms) / 1000.0))


def test_event_driven_messages_are_counted_not_timed(network, plain, protected):
    frames = periodic_frames(protected, "SENSOR_HEARTBEAT", SENSOR_HEARTBEAT, 0.1, 5)
    frames.append(frame(1000.05, ECHO_REQUEST, plain("ECHO_REQUEST", target_node=0, seq=1, payload=2)))
    r = analysis.analyze(frames, network)
    assert [p.name for p in r.periods] == ["SENSOR_HEARTBEAT"]
    assert r.events == {"ECHO_REQUEST": 1}


def test_unknown_identifiers_are_reported_and_still_count_as_load(network, protected):
    frames = periodic_frames(protected, "SENSOR_DATA", SENSOR_DATA, 0.010, 11)
    frames.append(frame(1000.05, 0x001, b"\x00" * 8))
    r = analysis.analyze(frames, network)
    assert r.unknown == {0x001: 1}
    assert [row.name for row in r.load.rows] == ["SENSOR_DATA", "0x001"]


def test_protection_verdicts_follow_the_receiver_policy(network, protected):
    good = [protected("SENSOR_DATA", c) for c in (0, 1, 2)]
    skipped = protected("SENSOR_DATA", 4)            # delta 2: accepted with one frame lost
    repeated = protected("SENSOR_DATA", 4)           # delta 0
    stale = protected("SENSOR_DATA", 15)             # delta 11: out of sequence, reference stays at 4
    bad_crc = bytearray(protected("SENSOR_DATA", 5))
    bad_crc[0] ^= 0x01
    short = protected("SENSOR_DATA", 5)[:7]
    data = good + [skipped, repeated, stale, bytes(bad_crc), short, protected("SENSOR_DATA", 5)]
    frames = [frame(1000.0 + 0.01 * i, SENSOR_DATA, d) for i, d in enumerate(data)]
    r = analysis.analyze(frames, network)
    assert (r.protected, r.dlc_bad, r.crc_bad, r.repeated, r.out_of_sequence, r.lost) == (9, 1, 1, 1, 1, 1)

    checker = E2EChecker(network)
    verdicts = [checker.check(SENSOR_DATA, d) for d in data]
    assert [v.counter_check for v in verdicts] == ["ok", "ok", "ok", "skipped", "repeated", "out_of_sequence",
                                                   None, None, "ok"]
    assert [v.fault for v in verdicts] == [None, None, None, None, "repeated", "out_of_sequence", "crc", "dlc", None]
    assert [v.accepted for v in verdicts] == [True, True, True, True, False, False, False, False, True]
    assert verdicts[3].lost == 1
    # An unprotected message only has to have the right length.
    assert checker.check(ECHO_REQUEST, b"\x00" * 6).accepted
    assert checker.check(ECHO_REQUEST, b"\x00" * 5).fault == "dlc"


def echo_frames(plain, requests, responses):
    """requests: (t, target, seq, payload); responses: (t, responder_id, seq, payload, turnaround_us)."""
    frames = [frame(t, ECHO_REQUEST, plain("ECHO_REQUEST", target_node=target, seq=seq, payload=payload))
              for t, target, seq, payload in requests]
    for t, rid, seq, payload, turnaround in responses:
        name = "SENSOR_ECHO_RESPONSE" if rid == SENSOR_ECHO else "CONTROLLER_ECHO_RESPONSE"
        frames.append(frame(t, rid, plain(name, seq=seq, payload=payload, turnaround=turnaround)))
    return frames


def test_echo_matching_rtt_and_turnaround(network, plain):
    requests = [(1000.0 + 0.02 * i, 0, i & 0xFF, 0xABCD0000 | i) for i in range(300)]   # seq wraps at 256
    responses = []
    for t, _, seq, payload in requests:
        responses.append((t + 0.0005, SENSOR_ECHO, seq, payload, 100))
        if seq != 7:   # the controller misses one probe
            responses.append((t + 0.0007, CONTROLLER_ECHO, seq, payload, 250))
    responses.append((1000.1, CONTROLLER_ECHO, 200, 0xDEAD0000, 1))                    # stray: no request
    rows = {r.responder: r for r in analysis.match_echo(echo_frames(plain, requests, responses), network)}
    sensor, controller = rows["SENSOR_ECU"], rows["CONTROLLER_ECU"]
    assert (sensor.sent, sensor.answered, sensor.unmatched) == (300, 300, 0)
    assert (controller.sent, controller.answered, controller.unmatched) == (300, 298, 1)
    assert sensor.rtt(50) == pytest.approx(0.5, abs=1e-6)
    assert sensor.rtt(100) == pytest.approx(0.5, abs=1e-6)
    assert controller.rtt(50) == pytest.approx(0.7, abs=1e-6)
    assert sensor.turnaround(50) == pytest.approx(0.1)
    assert controller.rest(50) == pytest.approx(0.45, abs=1e-6)


def test_echo_targets_one_node(network, plain):
    requests = [(1000.0, 1, 0, 1), (1000.1, 2, 1, 2)]   # SENSOR_ECU, then CONTROLLER_ECU
    responses = [(1000.001, SENSOR_ECHO, 0, 1, 10), (1000.101, CONTROLLER_ECHO, 1, 2, 10),
                 (1000.102, SENSOR_ECHO, 1, 2, 10)]      # the sensor answering a probe not addressed to it
    rows = {r.responder: r for r in analysis.match_echo(echo_frames(plain, requests, responses), network)}
    assert (rows["SENSOR_ECU"].sent, rows["SENSOR_ECU"].answered, rows["SENSOR_ECU"].unmatched) == (1, 1, 1)
    assert (rows["CONTROLLER_ECU"].sent, rows["CONTROLLER_ECU"].answered) == (1, 1)


def test_report_text_and_json(network, protected, plain):
    frames = periodic_frames(protected, "SENSOR_DATA", SENSOR_DATA, 0.010, 101)
    frames += periodic_frames(protected, "CONTROLLER_COMMAND", CONTROLLER_COMMAND, 0.020, 51)
    frames += echo_frames(plain, [(1000.3, 0, 1, 5)], [(1000.3004, SENSOR_ECHO, 1, 5, 40)])
    r = analysis.analyze(frames, network, source="synthetic")
    text = analysis.format_report(r)
    assert "simulated bus" in text and "SENSOR_DATA" in text and "CONTROLLER_COMMAND" in text
    assert "SENSOR_ECU" in text and "no response" in text      # the controller never answered
    d = analysis.to_dict(r)
    assert d["frames"] == len(frames) and d["simulated_bus"] is True
    assert {p["name"] for p in d["periods"]} == {"SENSOR_DATA", "CONTROLLER_COMMAND"}
    assert d["echo"][1]["responder"] == "SENSOR_ECU" and d["echo"][1]["rtt_ms"]["p50"] == pytest.approx(0.4, abs=1e-6)
    assert math.isclose(d["bus_load"]["nominal_load_pct"], 3.996)
    assert analysis.format_echo([]).startswith("echo round trip: no ECHO_REQUEST")


def test_analyze_rejects_an_empty_capture(network):
    with pytest.raises(ValueError):
        analysis.analyze([], network)

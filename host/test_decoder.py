"""Tests of host.decoder: decoding, statistics, stream/table output, log files (run: pytest host/)."""

import pytest

from host import decoder
from host.capture import read_log
from host.conftest import frame

SENSOR_DATA, ECHO_REQUEST = 0x100, 0x300


def test_decoded_values_period_and_verdict(network, protected):
    d = decoder.Decoder(network)
    first = d.feed(frame(1000.000, SENSOR_DATA, protected("SENSOR_DATA", 3, speed=12.34, yaw_rate=-1.2, accel_x=0.5)))
    second = d.feed(frame(1000.010, SENSOR_DATA, protected("SENSOR_DATA", 4, speed=12.35, sensor_status=1)))
    assert first.name == "SENSOR_DATA" and first.period_ms is None
    assert first.values["speed"] == pytest.approx(12.34) and first.values["yaw_rate"] == pytest.approx(-1.2)
    assert first.values["sensor_status"] == "OK" and second.values["sensor_status"] == "DEGRADED"
    assert second.period_ms == pytest.approx(10.0)
    assert (second.verdict.protected, second.verdict.crc_ok, second.verdict.counter, second.verdict.counter_check) \
        == (True, True, 4, "ok")
    assert first.values_text() == "sensor_status=OK speed=12.34 m/s yaw_rate=-1.2 deg/s accel_x=0.50 m/s^2"
    st = d.stats[SENSOR_DATA]
    assert st.count == 2 and st.mean_period_ms == pytest.approx(10.0) and st.max_period_ms == pytest.approx(10.0)
    assert st.rate_hz(1000.010, 1.0) == 2.0


def test_unknown_and_unprotected_frames(network, plain):
    d = decoder.Decoder(network)
    unknown = d.feed(frame(1.0, 0x7FF, b"\x01\x02"))
    assert unknown.name == "0x7FF" and unknown.message is None and unknown.values == {} and unknown.verdict.short() == "-"
    echo = d.feed(frame(1.0, ECHO_REQUEST, plain("ECHO_REQUEST", target_node=2, seq=9, payload=77)))
    assert echo.values == {"target_node": "CONTROLLER_ECU", "seq": 9, "payload": 77}
    assert echo.verdict.protected is False and echo.verdict.accepted
    short = d.feed(frame(1.1, ECHO_REQUEST, b"\x00" * 3))
    assert short.verdict.fault == "dlc" and short.values == {} and d.stats[ECHO_REQUEST].dlc_bad == 1


def test_stream_line_and_table_text(network, protected):
    d = decoder.Decoder(network)
    d.feed(frame(1000.000, SENSOR_DATA, protected("SENSOR_DATA", 0, speed=1.0)))
    line = decoder.format_stream_line(d.feed(frame(1000.010, SENSOR_DATA, protected("SENSOR_DATA", 1, speed=1.0))))
    assert line.startswith("(1000.010000) SENSOR_DATA") and "0x100 [8]" in line and "+  10.00 ms" in line
    assert "ctr  1 ok" in line and "speed=1.00 m/s" in line
    bad = bytearray(protected("SENSOR_DATA", 2))
    bad[3] ^= 0x80
    line = decoder.format_stream_line(d.feed(frame(1000.020, SENSOR_DATA, bytes(bad))))
    assert "crc BAD" in line
    table = d.table()
    assert "SENSOR_DATA" in table and " 0x100 " in table
    row = next(l for l in table.splitlines() if l.startswith("SENSOR_DATA"))
    assert row.split()[2] == "3"           # count
    assert "   1    0    0     0" in row    # crc 1, rep 0, oos 0, lost 0


def test_cli_decodes_a_log_file_with_and_without_direction_marker(network, protected, tmp_path, capsys):
    log = tmp_path / "bus.log"
    log.write_text(f"(1700000000.000000) vcan0 100#{protected('SENSOR_DATA', 0, speed=2.5).hex().upper()}\n"
                   f"(1700000000.010000) vcan0 100#{protected('SENSOR_DATA', 1, speed=2.5).hex().upper()} R\n"
                   f"(1700000000.020000) vcan0 300#000102030405 R\n")
    assert len(read_log(log)) == 3
    assert decoder.main(["--file", str(log), "--mode", "stream", "--ids", "SENSOR_DATA"]) == 0
    out = capsys.readouterr().out.splitlines()
    assert len(out) == 2 and all("SENSOR_DATA" in l and "speed=2.50 m/s" in l for l in out)
    assert decoder.main(["--file", str(log), "--mode", "table"]) == 0
    out = capsys.readouterr().out
    assert "SENSOR_DATA" in out and "ECHO_REQUEST" in out and "simulated bus" in out
    assert decoder.main(["--file", str(log), "--every", "2"]) == 0
    assert len(capsys.readouterr().out.splitlines()) == 2   # first SENSOR_DATA and the ECHO_REQUEST


def test_ids_accept_names_and_numbers(network):
    assert decoder.parse_ids(network, "SENSOR_DATA, 0x080,513") == {0x100, 0x080, 0x201}
    assert decoder.parse_ids(network, None) is None

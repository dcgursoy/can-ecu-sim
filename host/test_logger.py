"""Tests of host.logger on python-can's in-process virtual interface (run: pytest host/)."""

import csv
import threading
import time

import can
import pytest

from host import logger
from host.capture import read_log

SENSOR_DATA, ECHO_REQUEST = 0x100, 0x300


@pytest.fixture
def channel():
    return f"host-logger-test-{time.monotonic_ns()}"


def test_recorder_writes_log_and_decoded_csv(network, protected, plain, tmp_path, channel):
    rx = can.Bus(interface="virtual", channel=channel)
    tx = can.Bus(interface="virtual", channel=channel)
    recorder = logger.Recorder(network, tmp_path, channel)
    stop = threading.Event()
    worker = threading.Thread(target=logger.run, args=(rx, recorder, 0.0, stop))
    worker.start()
    try:
        sent = [protected("SENSOR_DATA", c, speed=12.34, yaw_rate=-1.2, accel_x=0.5) for c in (0, 1, 3)]  # 3: skipped
        for data in sent:
            tx.send(can.Message(arbitration_id=SENSOR_DATA, data=data, is_extended_id=False))
            time.sleep(0.01)
        tx.send(can.Message(arbitration_id=ECHO_REQUEST, data=plain("ECHO_REQUEST", target_node=0, seq=1, payload=2),
                            is_extended_id=False))
        tx.send(can.Message(arbitration_id=0x123, data=b"\x01", is_extended_id=True))   # 29-bit: ignored
        deadline = time.monotonic() + 2.0
        while recorder.frames < 4 and time.monotonic() < deadline:
            time.sleep(0.01)
    finally:
        stop.set()
        worker.join(timeout=2.0)
        tx.shutdown()
        rx.shutdown()
    assert recorder.frames == 4

    frames = read_log(tmp_path / "bus.log")
    assert [f.can_id for f in frames] == [SENSOR_DATA] * 3 + [ECHO_REQUEST]
    assert [f.data for f in frames[:3]] == sent
    lines = (tmp_path / "bus.log").read_text().splitlines()
    assert lines[0].startswith("(") and f" {channel} 100#" in lines[0]

    with open(tmp_path / "bus.csv", newline="") as f:
        rows = list(csv.DictReader(f))
    assert list(rows[0].keys())[:11] == logger.FIXED_COLUMNS
    assert "SENSOR_DATA.speed" in rows[0] and "CONTROLLER_COMMAND.mode" in rows[0]
    assert [r["name"] for r in rows] == ["SENSOR_DATA"] * 3 + ["ECHO_REQUEST"]
    assert rows[0]["id"] == "0x100" and rows[0]["dlc"] == "8" and rows[0]["data"] == sent[0].hex().upper()
    assert rows[0]["protected"] == "1" and rows[0]["crc_ok"] == "1" and rows[0]["counter"] == "0"
    assert [r["counter_check"] for r in rows] == ["ok", "ok", "skipped", ""]
    assert rows[0]["period_ms"] == "" and float(rows[1]["period_ms"]) > 0
    assert float(rows[0]["SENSOR_DATA.speed"]) == pytest.approx(12.34)
    assert rows[0]["SENSOR_DATA.sensor_status"] == "OK"
    assert rows[3]["ECHO_REQUEST.target_node"] == "ALL" and rows[3]["SENSOR_DATA.speed"] == ""
    assert rows[3]["protected"] == "0" and rows[3]["crc_ok"] == ""

    summary = recorder.summary()
    assert "captured 4 frames" in summary and "SENSOR_DATA" in summary and "lost 1" in summary


def test_cli_records_for_a_duration(network, protected, tmp_path, channel):
    tx = can.Bus(interface="virtual", channel=channel)
    result = {}

    def feed():
        time.sleep(0.2)
        for c in range(5):
            tx.send(can.Message(arbitration_id=SENSOR_DATA, data=protected("SENSOR_DATA", c), is_extended_id=False))
            time.sleep(0.01)

    feeder = threading.Thread(target=feed)
    feeder.start()
    try:
        result["rc"] = logger.main(["--iface", channel, "--interface", "virtual", "--out", str(tmp_path),
                                    "--duration", "0.8", "--no-csv", "--quiet"])
    finally:
        feeder.join()
        tx.shutdown()
    assert result["rc"] == 0
    assert len(read_log(tmp_path / "bus.log")) == 5
    assert not (tmp_path / "bus.csv").exists()

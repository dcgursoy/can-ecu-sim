"""Tests of host.echo against a fake responder on the virtual interface (run: pytest host/)."""

import threading
import time

import can
import pytest

from host import analysis, echo
from host.network import choice_value

ECHO_REQUEST, SENSOR_ECHO = 0x300, 0x301


class FakeSensor(threading.Thread):
    """Answers every ECHO_REQUEST addressed to ALL or SENSOR_ECU with a SENSOR_ECHO_RESPONSE."""

    def __init__(self, network, channel: str, turnaround_us: int = 123) -> None:
        super().__init__(daemon=True)
        self.bus = can.Bus(interface="virtual", channel=channel)
        self.req = network.by_name["ECHO_REQUEST"]
        self.resp = network.by_name["SENSOR_ECHO_RESPONSE"]
        self.me = {choice_value(self.req.get_signal_by_name("target_node"), n) for n in ("ALL", "SENSOR_ECU")}
        self.turnaround_us = turnaround_us
        self.stop = threading.Event()
        self.answered = 0

    def run(self) -> None:
        while not self.stop.is_set():
            msg = self.bus.recv(timeout=0.05)
            if msg is None or msg.arbitration_id != ECHO_REQUEST:
                continue
            req = self.req.decode(bytes(msg.data), decode_choices=False)
            if req["target_node"] not in self.me:
                continue
            data = self.resp.encode({"seq": req["seq"], "payload": req["payload"], "turnaround": self.turnaround_us})
            self.bus.send(can.Message(arbitration_id=SENSOR_ECHO, data=data, is_extended_id=False))
            self.answered += 1

    def close(self) -> None:
        self.stop.set()
        self.join(timeout=2.0)
        self.bus.shutdown()


def test_probe_measures_round_trips_from_bus_timestamps(network):
    channel = f"host-echo-test-{time.monotonic_ns()}"
    responder = FakeSensor(network, channel)
    responder.start()
    bus = can.Bus(interface="virtual", channel=channel, receive_own_messages=True)
    try:
        frames = echo.EchoProbe(bus, network, "ALL", seed=1).run(count=5, interval_s=0.01, timeout_s=0.3)
    finally:
        bus.shutdown()
        responder.close()
    assert responder.answered == 5
    requests = [f for f in frames if f.can_id == ECHO_REQUEST]
    assert len(requests) == 5 and all(not f.is_rx for f in requests)      # our own transmissions, looped back
    seqs = [network.decode(network.by_name["ECHO_REQUEST"], f.data)["seq"] for f in requests]
    assert seqs == [0, 1, 2, 3, 4]
    rows = {r.responder: r for r in analysis.match_echo(frames, network)}
    sensor, controller = rows["SENSOR_ECU"], rows["CONTROLLER_ECU"]
    assert (sensor.sent, sensor.answered, sensor.unmatched) == (5, 5, 0)
    assert (controller.sent, controller.answered) == (5, 0)               # nobody plays the controller here
    assert all(r > 0 for r in sensor.rtt_ms) and sensor.rtt(50) < 100.0
    assert sensor.turnaround(50) == pytest.approx(0.123)
    text = analysis.format_echo(rows.values())
    assert "SENSOR_ECU" in text and "no response" in text


def test_payload_is_unique_per_probe_and_target_is_encoded(network):
    bus = can.Bus(interface="virtual", channel=f"host-echo-enc-{time.monotonic_ns()}")
    try:
        probe = echo.EchoProbe(bus, network, "CONTROLLER_ECU", seed=7)
        req = network.by_name["ECHO_REQUEST"]
        decoded = [network.decode(req, probe.request(i).data) for i in (0, 255, 256, 70000)]
    finally:
        bus.shutdown()
    assert [d["seq"] for d in decoded] == [0, 255, 0, 70000 & 0xFF]
    assert all(d["target_node"] == "CONTROLLER_ECU" for d in decoded)
    assert len({d["payload"] for d in decoded[:3]}) == 3
    assert decoded[0]["payload"] >> 16 == probe.nonce

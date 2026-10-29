"""Shared fixtures for the host-tool tests (run: pytest host/). No vcan needed:
the live tests use python-can's in-process "virtual" interface."""

import pytest

from host.capture import Frame
from host.network import Network, e2e_ref


@pytest.fixture(scope="session")
def network() -> Network:
    return Network()


@pytest.fixture(scope="session")
def protected(network):
    """protected("SENSOR_DATA", counter, speed=12.34) -> bytes, protected like the firmware does."""

    def build(name: str, counter: int, **signals) -> bytes:
        msg = network.by_name[name]
        values = {s.name: 0 for s in msg.signals}
        values.update(signals)
        return e2e_ref.protect(msg.frame_id, msg.encode(values), counter)

    return build


@pytest.fixture(scope="session")
def plain(network):
    """plain("ECHO_REQUEST", seq=1, payload=2, target_node=0) -> bytes (no protection)."""

    def build(name: str, **signals) -> bytes:
        msg = network.by_name[name]
        values = {s.name: 0 for s in msg.signals}
        values.update(signals)
        return msg.encode(values)

    return build


def frame(t: float, can_id: int, data: bytes, channel: str = "vcan0") -> Frame:
    return Frame(t, can_id, data, channel)

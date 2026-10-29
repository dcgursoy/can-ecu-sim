"""Frame sources: a live interface through python-can, or a recorded log."""

from __future__ import annotations

import dataclasses
import pathlib
import signal
import threading

import can


@dataclasses.dataclass(frozen=True)
class Frame:
    """One standard data frame as the host saw it."""

    timestamp: float    # seconds; from a SocketCAN interface the kernel's receive timestamp (CLOCK_REALTIME)
    can_id: int
    data: bytes
    channel: str = ""
    is_rx: bool = True  # False: this socket's own transmission, looped back to it. python-can derives
    #                     it from the kernel's MSG_DONTROUTE flag, which is set for every frame that
    #                     originated on the local host, so on vcan it is False for all traffic;
    #                     the tools here never rely on it.

    @property
    def dlc(self) -> int:
        return len(self.data)

    @classmethod
    def from_message(cls, msg: can.Message) -> "Frame":
        channel = msg.channel if isinstance(msg.channel, str) else ("" if msg.channel is None else str(msg.channel))
        return cls(float(msg.timestamp or 0.0), int(msg.arbitration_id), bytes(msg.data), channel, bool(msg.is_rx))

    def to_message(self) -> can.Message:
        return can.Message(timestamp=self.timestamp, arbitration_id=self.can_id, data=self.data,
                           is_extended_id=False, channel=self.channel or None, is_rx=self.is_rx)


def usable(msg: can.Message) -> bool:
    """Standard data frames only: the network has no remote, error or 29-bit frames."""
    return not (msg.is_error_frame or msg.is_remote_frame or msg.is_extended_id)


def open_bus(channel: str, interface: str = "socketcan", receive_own_messages: bool = False) -> can.BusABC:
    try:
        return can.Bus(channel=channel, interface=interface, receive_own_messages=receive_own_messages)
    except (OSError, can.CanError) as exc:
        raise SystemExit(f"cannot open CAN interface {channel!r} ({exc}); inside the dev container, "
                         f"create it with tools/vcan-up.sh {channel}") from exc


def read_log(path: str | pathlib.Path) -> list[Frame]:
    """Every standard data frame of a recorded log, in file order. python-can reads
    the candump .log format (with or without the newer R/T direction marker) and
    its other formats (.asc, .blf, .trc, .csv)."""
    frames = []
    with can.LogReader(str(path)) as reader:
        for msg in reader:
            if usable(msg):
                frames.append(Frame.from_message(msg))
    return frames


def install_stop_handlers(stop: threading.Event):
    """Make SIGINT and SIGTERM set `stop` (main thread only; a no-op elsewhere).
    Returns a function that restores the previous handlers."""
    if threading.current_thread() is not threading.main_thread():
        return lambda: None
    previous = {sig: signal.signal(sig, lambda *_: stop.set()) for sig in (signal.SIGINT, signal.SIGTERM)}

    def restore() -> None:
        for sig, handler in previous.items():
            signal.signal(sig, handler)

    return restore


def receive(bus: can.BusABC, duration_s: float, stop: threading.Event):
    """Yield usable frames from `bus` until `duration_s` elapses (0: no limit) or `stop` is set."""
    import time
    deadline = time.monotonic() + duration_s if duration_s > 0 else None
    while not stop.is_set() and (deadline is None or time.monotonic() < deadline):
        msg = bus.recv(timeout=0.2)
        if msg is not None and usable(msg):
            yield msg

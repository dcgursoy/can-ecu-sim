#!/usr/bin/env python3
"""Fault-injection gateway between two SocketCAN interfaces.

    python3 -m gateway.gateway --a vcan0 --b vcan1 [--seed 1] [--control] [--stats-file PATH]
        [--a-to-b JSON] [--b-to-a JSON] [--both JSON] [--flood JSON]

Every standard data frame received on interface A is forwarded to B and vice
versa, through one gateway.faults.FaultEngine per direction. Two sockets are
used, one bound to each interface, and each direction reads from one and writes
to the other. A SocketCAN socket never receives the frames it sent itself
(CAN_RAW_RECV_OWN_MSGS is off), so nothing the gateway forwards comes back to
it: no loop, and no marker in the frames.

Fault JSON objects take the fields of gateway.faults.FaultConfig, e.g.
    '{"drop": 0.1}'                                  lose 10 % of the frames
    '{"delay_ms": 20, "jitter_ms": 5}'               20 +/- 5 ms latency
    '{"reorder": 0.2, "ids": ["0x100"]}'             swap 20 % of SENSOR_DATA with their successor
    '{"corrupt": 0.1}'                               flip one bit in 10 % of the frames
A flood is a generator, not a per-frame fault:
    '{"iface": "b", "id": "0x001", "rate": 2000, "dlc": 8}'
It transmits on one segment only (its socket does not see its own frames), as a
babbling node on that segment would.

With --control the gateway reads one JSON command per line from stdin and
answers each with one JSON line on stdout, so a test driver can change the
faults while the ECUs keep running:
    {"a_to_b": {...}} {"b_to_a": {...}} {"both": {...}}   replace a direction's faults
    {"flood": {...}} / {"flood": null}                     start / stop the flood
    {"stats": true} / {"stats": "reset"}                   report (and reset) the counters
    {"quit": true}                                         stop
Counters are also written to --stats-file every 0.5 s and at exit, and printed
to stderr at exit. Timing is Python on a desktop OS: the relay adds latency of
its own (tens to hundreds of microseconds, more when the host is loaded), which
host/ tooling measures as part of the echo round trip (Phase 4).
"""

from __future__ import annotations

import argparse
import errno
import json
import select
import signal
import socket
import struct
import sys
import threading
import time

from gateway.faults import FaultConfig, FaultEngine, Frame

CAN_FRAME = struct.Struct("=IB3x8s")  # struct can_frame: id/flags, dlc, pad, data
CAN_EFF_FLAG = 0x8000_0000
CAN_RTR_FLAG = 0x4000_0000
CAN_ERR_FLAG = 0x2000_0000
CAN_SFF_MASK = 0x7FF
STATS_PERIOD_S = 0.5
IDLE_POLL_S = 0.05


def open_can_socket(iface: str) -> socket.socket:
    sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    sock.bind((iface,))
    sock.setblocking(False)
    return sock


def send_frame(sock: socket.socket, frame: Frame, retries: int = 3) -> bool:
    """Transmit; retry briefly when the interface queue is full. False on failure."""
    raw = CAN_FRAME.pack(frame.can_id, len(frame.data), frame.data.ljust(8, b"\0"))
    for attempt in range(retries + 1):
        try:
            sock.send(raw)
            return True
        except (BlockingIOError, InterruptedError):
            pass
        except OSError as e:
            if e.errno not in (errno.ENOBUFS, errno.EAGAIN):
                return False
        if attempt < retries:
            time.sleep(0.001)
    return False


class Direction(threading.Thread):
    """One forwarding direction: a receive socket, a transmit socket and a fault engine."""

    def __init__(self, name: str, rx: socket.socket, tx: socket.socket, engine: FaultEngine) -> None:
        super().__init__(name=name, daemon=True)
        self.rx = rx
        self.tx = tx
        self.engine = engine
        self.lock = threading.Lock()  # engine access from this thread and the control thread
        self.stop_event = threading.Event()
        self.tx_errors = 0
        self.ignored = 0  # extended, remote and error frames are not forwarded

    def run(self) -> None:
        while not self.stop_event.is_set():
            with self.lock:
                next_release = self.engine.next_release()
            now = time.monotonic()
            timeout = IDLE_POLL_S if next_release is None else max(0.0, min(IDLE_POLL_S, next_release - now))
            readable, _, _ = select.select([self.rx], [], [], timeout)
            now = time.monotonic()
            if readable:
                self._receive_all(now)
            with self.lock:
                due = list(self.engine.due(now))
            for frame in due:
                if not send_frame(self.tx, frame):
                    self.tx_errors += 1

    def _receive_all(self, now: float) -> None:
        while True:
            try:
                raw = self.rx.recv(CAN_FRAME.size)
            except (BlockingIOError, InterruptedError):
                return
            except OSError:
                return
            if len(raw) < CAN_FRAME.size:
                continue
            can_id, dlc, data = CAN_FRAME.unpack(raw)
            if can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG):
                self.ignored += 1
                continue
            with self.lock:
                self.engine.submit(Frame(can_id & CAN_SFF_MASK, data[: min(dlc, 8)]), now)

    def configure(self, config: FaultConfig) -> None:
        with self.lock:
            self.engine.configure(config)

    def stats(self, reset: bool = False) -> dict:
        with self.lock:
            d = self.engine.stats.to_dict()
            d["pending"] = self.engine.pending()
            d["config"] = self.engine.config.to_dict()
            if reset:
                self.engine.reset_stats()
        d["tx_errors"] = self.tx_errors
        d["ignored"] = self.ignored
        if reset:
            self.tx_errors = 0
            self.ignored = 0
        return d


class Flood(threading.Thread):
    """Transmit frames with one identifier at a fixed rate on one socket."""

    def __init__(self, sock: socket.socket, can_id: int, rate_hz: float, dlc: int) -> None:
        super().__init__(name="flood", daemon=True)
        if rate_hz <= 0.0:
            raise ValueError("flood rate must be positive")
        self.sock = sock
        self.can_id = can_id
        self.period = 1.0 / rate_hz
        self.dlc = max(0, min(8, dlc))
        self.stop_event = threading.Event()
        self.sent = 0
        self.errors = 0
        self.started_at = 0.0
        self.stopped_at = 0.0

    def run(self) -> None:
        self.started_at = time.monotonic()
        next_t = self.started_at
        seq = 0
        while not self.stop_event.is_set():
            now = time.monotonic()
            if now < next_t:
                time.sleep(min(next_t - now, 0.002))
                continue
            payload = (struct.pack("<I", seq & 0xFFFF_FFFF) + b"\xAA\x55\xAA\x55")[: self.dlc]
            if send_frame(self.sock, Frame(self.can_id, payload), retries=0):
                self.sent += 1
            else:
                self.errors += 1
            seq += 1
            next_t += self.period
            if next_t < now - 0.05:
                next_t = now  # fell far behind (host stall): do not burst to catch up
        self.stopped_at = time.monotonic()

    def stats(self) -> dict:
        end = self.stopped_at or time.monotonic()
        span = max(end - self.started_at, 1e-9) if self.started_at else 0.0
        return {"id": f"0x{self.can_id:03X}", "sent": self.sent, "errors": self.errors,
                "achieved_rate_hz": round(self.sent / span, 1) if span else 0.0, "span_s": round(span, 3)}


class Gateway:
    def __init__(self, iface_a: str, iface_b: str, seed: int) -> None:
        self.iface_a = iface_a
        self.iface_b = iface_b
        self.sock_a = open_can_socket(iface_a)
        self.sock_b = open_can_socket(iface_b)
        self.a_to_b = Direction(f"{iface_a}->{iface_b}", self.sock_a, self.sock_b, FaultEngine(seed=seed))
        self.b_to_a = Direction(f"{iface_b}->{iface_a}", self.sock_b, self.sock_a, FaultEngine(seed=seed + 1))
        self.flood: Flood | None = None
        self.last_flood: dict | None = None
        self.started_at = time.monotonic()

    def start(self) -> None:
        self.a_to_b.start()
        self.b_to_a.start()

    def stop(self) -> None:
        self.stop_flood()
        for d in (self.a_to_b, self.b_to_a):
            d.stop_event.set()
        for d in (self.a_to_b, self.b_to_a):
            d.join(timeout=1.0)
        self.sock_a.close()
        self.sock_b.close()

    def start_flood(self, spec: dict) -> dict:
        self.stop_flood()
        iface = spec.get("iface", "b")
        sock = {"a": self.sock_a, "b": self.sock_b}[iface]
        can_id = spec.get("id", 1)
        can_id = int(can_id, 0) if isinstance(can_id, str) else int(can_id)
        self.flood = Flood(sock, can_id, float(spec.get("rate", 1000)), int(spec.get("dlc", 8)))
        self.flood.start()
        return self.flood.stats()

    def stop_flood(self) -> dict | None:
        if self.flood is None:
            return self.last_flood
        self.flood.stop_event.set()
        self.flood.join(timeout=1.0)
        self.last_flood = self.flood.stats()
        self.flood = None
        return self.last_flood

    def stats(self, reset: bool = False) -> dict:
        flood = self.flood.stats() if self.flood is not None else self.last_flood
        out = {"uptime_s": round(time.monotonic() - self.started_at, 3),
               "a_to_b": self.a_to_b.stats(reset), "b_to_a": self.b_to_a.stats(reset), "flood": flood}
        if reset:
            self.last_flood = None
        return out

    def command(self, cmd: dict) -> dict:
        """One control command; returns the reply object."""
        reply: dict = {"ok": True}
        if "both" in cmd:
            config = FaultConfig.from_dict(cmd["both"] or {})
            self.a_to_b.configure(config)
            self.b_to_a.configure(config)
        if "a_to_b" in cmd:
            self.a_to_b.configure(FaultConfig.from_dict(cmd["a_to_b"] or {}))
        if "b_to_a" in cmd:
            self.b_to_a.configure(FaultConfig.from_dict(cmd["b_to_a"] or {}))
        if "flood" in cmd:
            reply["flood"] = self.start_flood(cmd["flood"]) if cmd["flood"] else self.stop_flood()
        if "stats" in cmd and cmd["stats"]:
            reply["stats"] = self.stats(reset=cmd["stats"] == "reset")
        if cmd.get("quit"):
            reply["quit"] = True
        return reply


def parse_fault_json(text: str | None) -> FaultConfig | None:
    return FaultConfig.from_dict(json.loads(text)) if text else None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--a", default="vcan0", help="interface A (default vcan0)")
    parser.add_argument("--b", default="vcan1", help="interface B (default vcan1)")
    parser.add_argument("--seed", type=int, default=1, help="random seed of the fault decisions")
    parser.add_argument("--a-to-b", help="fault JSON for the A->B direction")
    parser.add_argument("--b-to-a", help="fault JSON for the B->A direction")
    parser.add_argument("--both", help="fault JSON for both directions")
    parser.add_argument("--flood", help="flood JSON: iface, id, rate, dlc")
    parser.add_argument("--control", action="store_true", help="read JSON commands from stdin")
    parser.add_argument("--stats-file", help="write the counters here every 0.5 s and at exit")
    args = parser.parse_args(argv)

    gw = Gateway(args.a, args.b, args.seed)
    both = parse_fault_json(args.both)
    if both is not None:
        gw.a_to_b.configure(both)
        gw.b_to_a.configure(both)
    for direction, text in ((gw.a_to_b, args.a_to_b), (gw.b_to_a, args.b_to_a)):
        cfg = parse_fault_json(text)
        if cfg is not None:
            direction.configure(cfg)
    gw.start()
    if args.flood:
        gw.start_flood(json.loads(args.flood))

    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())

    def write_stats() -> None:
        if args.stats_file:
            tmp = args.stats_file + ".tmp"
            with open(tmp, "w") as f:
                json.dump(gw.stats(), f, indent=1)
            # rename: a reader never sees a half-written file
            import os
            os.replace(tmp, args.stats_file)

    def stats_writer() -> None:
        while not stop.wait(STATS_PERIOD_S):
            write_stats()

    if args.stats_file:
        threading.Thread(target=stats_writer, name="stats", daemon=True).start()

    print(f"gateway: {args.a} <-> {args.b} seed={args.seed} control={'on' if args.control else 'off'}",
          file=sys.stderr, flush=True)
    rc = 0
    try:
        if args.control:
            # One JSON object per line; EOF on stdin stops the gateway.
            while not stop.is_set():
                readable, _, _ = select.select([sys.stdin], [], [], 0.1)
                if not readable:
                    continue
                line = sys.stdin.readline()
                if not line:
                    break
                line = line.strip()
                if not line:
                    continue
                try:
                    reply = gw.command(json.loads(line))
                except (ValueError, KeyError, TypeError) as e:
                    reply = {"ok": False, "error": f"{type(e).__name__}: {e}"}
                print(json.dumps(reply), flush=True)
                if reply.get("quit"):
                    break
        else:
            while not stop.wait(0.2):
                pass
    finally:
        gw.stop()
        final = gw.stats()
        write_stats()
        print("gateway stats: " + json.dumps(final), file=sys.stderr, flush=True)
    return rc


if __name__ == "__main__":
    sys.exit(main())

"""The fault engine of the gateway: pure, deterministic decisions about every
frame that crosses one direction of the gateway, with no sockets or threads so
it can be unit-tested with a fake clock.

A direction is a FIFO link with a configurable latency. Every frame that enters
it is scheduled for release at

    t_release = max(t_arrival + delay_ms +/- jitter_ms, t_release of the previous frame)

so variable latency alone never reorders frames (a store-and-forward link cannot
overtake itself); reordering is a fault of its own. Faults are decided per frame
from a seeded random generator, so a scenario is reproducible from its seed:

    drop       the frame is not forwarded
    duplicate  the frame is forwarded twice, back to back
    corrupt    one bit of one data byte (or of byte 0, the E2E CRC) is flipped
    reorder    the frame is held back and released right after the next frame
               with the same identifier (a swap of two consecutive frames); if
               no successor arrives within `reorder_hold_ms` it is released late

`ids` restricts all of a direction's faults to those identifiers (None: every
frame); frames outside the set are forwarded with the plain delay. Flooding is
not a per-frame fault and lives in the relay (gateway.py).

docs/fault-model.md maps each fault to the hardware behaviour it stands in for.
"""

from __future__ import annotations

import dataclasses
import heapq
import random
from typing import Iterator


@dataclasses.dataclass(frozen=True)
class FaultConfig:
    """Faults applied to one direction. Probabilities are per frame, in [0, 1]."""

    delay_ms: float = 0.0
    jitter_ms: float = 0.0  # uniform in [-jitter_ms, +jitter_ms], added to delay_ms
    drop: float = 0.0
    duplicate: float = 0.0
    corrupt: float = 0.0
    corrupt_crc_only: bool = False  # flip a bit of byte 0 (the E2E CRC) instead of a random payload bit
    reorder: float = 0.0
    reorder_hold_ms: float = 50.0
    ids: frozenset[int] | None = None  # None: every frame

    def __post_init__(self) -> None:
        for name in ("drop", "duplicate", "corrupt", "reorder"):
            p = getattr(self, name)
            if not 0.0 <= p <= 1.0:
                raise ValueError(f"{name} must be a probability in [0, 1], got {p}")
        if self.delay_ms < 0.0 or self.jitter_ms < 0.0 or self.reorder_hold_ms < 0.0:
            raise ValueError("delay_ms, jitter_ms and reorder_hold_ms must be >= 0")
        if self.jitter_ms > self.delay_ms:
            raise ValueError("jitter_ms must not exceed delay_ms (latency cannot be negative)")

    @property
    def active(self) -> bool:
        return any((self.delay_ms, self.drop, self.duplicate, self.corrupt, self.reorder))

    @classmethod
    def from_dict(cls, d: dict) -> "FaultConfig":
        """Build from a JSON object; `ids` may be a list of ints or of hex strings."""
        kwargs = dict(d)
        ids = kwargs.pop("ids", None)
        if ids is not None:
            kwargs["ids"] = frozenset(int(i, 0) if isinstance(i, str) else int(i) for i in ids)
        unknown = set(kwargs) - {f.name for f in dataclasses.fields(cls)}
        if unknown:
            raise ValueError(f"unknown fault parameters: {sorted(unknown)}")
        return cls(**kwargs)

    def to_dict(self) -> dict:
        d = dataclasses.asdict(self)
        d["ids"] = sorted(self.ids) if self.ids is not None else None
        return d


@dataclasses.dataclass
class Frame:
    can_id: int
    data: bytes

    def __post_init__(self) -> None:
        if not 0 <= self.can_id <= 0x7FF:
            raise ValueError(f"standard identifier out of range: {self.can_id:#x}")
        if len(self.data) > 8:
            raise ValueError("classic CAN frames carry at most 8 bytes")


@dataclasses.dataclass
class Stats:
    received: int = 0
    forwarded: int = 0  # frames handed to the output, duplicates included
    dropped: int = 0
    duplicated: int = 0
    corrupted: int = 0
    reordered: int = 0  # frames held back and released after their successor
    released_late: int = 0  # held frames that saw no successor within reorder_hold_ms
    delayed: int = 0  # frames released later than they arrived (latency or hold)
    max_latency_ms: float = 0.0
    by_id: dict[int, dict[str, int]] = dataclasses.field(default_factory=dict)

    def count(self, can_id: int, key: str) -> None:
        self.by_id.setdefault(can_id, {})[key] = self.by_id.get(can_id, {}).get(key, 0) + 1

    def to_dict(self) -> dict:
        d = dataclasses.asdict(self)
        d["by_id"] = {f"0x{can_id:03X}": counters for can_id, counters in sorted(self.by_id.items())}
        return d


@dataclasses.dataclass(order=True)
class _Scheduled:
    t_release: float
    seq: int
    frame: Frame = dataclasses.field(compare=False)
    t_arrival: float = dataclasses.field(compare=False)
    cancelled: bool = dataclasses.field(default=False, compare=False)


class FaultEngine:
    """Per-direction fault decisions and release scheduling.

    `submit(frame, now)` records a frame that arrived at `now` (seconds, any
    monotonic clock); `due(now)` yields the frames whose release time has come,
    in release order; `next_release()` tells the caller how long it may sleep.
    """

    def __init__(self, config: FaultConfig | None = None, seed: int = 0) -> None:
        self._config = config or FaultConfig()
        self._rng = random.Random(seed)
        self._queue: list[_Scheduled] = []
        self._held: dict[int, _Scheduled] = {}  # can_id -> frame waiting for its successor
        self._last_release = float("-inf")
        self._seq = 0
        self.stats = Stats()

    @property
    def config(self) -> FaultConfig:
        return self._config

    def configure(self, config: FaultConfig) -> None:
        """Replace the fault configuration; frames already scheduled keep their fate."""
        self._config = config

    def reset_stats(self) -> None:
        self.stats = Stats()

    # --- input -------------------------------------------------------------------

    def submit(self, frame: Frame, now: float) -> None:
        stats = self.stats
        stats.received += 1
        stats.count(frame.can_id, "received")
        cfg = self._config
        faults_apply = cfg.ids is None or frame.can_id in cfg.ids

        # A frame of this identifier held back for reordering goes out right after
        # this one whatever happens to this one: the swap is the fault, not a loss.
        held = self._held.pop(frame.can_id, None)

        if faults_apply and cfg.drop > 0.0 and self._rng.random() < cfg.drop:
            stats.dropped += 1
            stats.count(frame.can_id, "dropped")
            if held is not None:
                self._release_after(held, self._latency(now, cfg))
            return

        if faults_apply and cfg.corrupt > 0.0 and frame.data and self._rng.random() < cfg.corrupt:
            frame = self._corrupt(frame, cfg.corrupt_crc_only)
            stats.corrupted += 1
            stats.count(frame.can_id, "corrupted")

        if faults_apply and held is None and cfg.reorder > 0.0 and self._rng.random() < cfg.reorder:
            # Hold it back: it leaves after the next frame with this identifier, or
            # at the hold deadline if none comes. Latency is accounted on release.
            self._held[frame.can_id] = self._schedule(frame, now + cfg.reorder_hold_ms / 1000.0, now,
                                                      account=False)
            stats.reordered += 1
            stats.count(frame.can_id, "reordered")
            return

        t_release = self._latency(now, cfg)
        self._schedule(frame, t_release, now)
        if held is not None:
            self._release_after(held, t_release)
        if faults_apply and cfg.duplicate > 0.0 and self._rng.random() < cfg.duplicate:
            self._schedule(Frame(frame.can_id, frame.data), t_release, now)
            stats.duplicated += 1
            stats.count(frame.can_id, "duplicated")

    # --- output ------------------------------------------------------------------

    def due(self, now: float) -> Iterator[Frame]:
        """Frames whose release time is <= now, in release order."""
        while self._queue and self._queue[0].t_release <= now:
            item = heapq.heappop(self._queue)
            if item.cancelled:
                continue
            if self._held.get(item.frame.can_id) is item:
                del self._held[item.frame.can_id]  # hold deadline reached without a successor
                self.stats.released_late += 1
                self._account(item.t_release - item.t_arrival)
            self.stats.forwarded += 1
            yield item.frame

    def next_release(self) -> float | None:
        """Release time of the earliest pending frame, or None when idle."""
        while self._queue and self._queue[0].cancelled:
            heapq.heappop(self._queue)
        return self._queue[0].t_release if self._queue else None

    def pending(self) -> int:
        return sum(1 for item in self._queue if not item.cancelled)

    # --- internals ---------------------------------------------------------------

    def _latency(self, now: float, cfg: FaultConfig) -> float:
        delay = cfg.delay_ms
        if cfg.jitter_ms > 0.0:
            delay += self._rng.uniform(-cfg.jitter_ms, cfg.jitter_ms)
        t = now + delay / 1000.0
        if t < self._last_release:
            t = self._last_release  # FIFO: a link never overtakes itself
        self._last_release = t
        return t

    def _account(self, latency_s: float) -> None:
        latency_ms = latency_s * 1000.0
        if latency_ms > 0.0005:
            self.stats.delayed += 1
        if latency_ms > self.stats.max_latency_ms:
            self.stats.max_latency_ms = latency_ms

    def _schedule(self, frame: Frame, t_release: float, t_arrival: float, account: bool = True) -> _Scheduled:
        self._seq += 1
        item = _Scheduled(t_release, self._seq, frame, t_arrival)
        heapq.heappush(self._queue, item)
        if account:
            self._account(t_release - t_arrival)
        return item

    def _release_after(self, held: _Scheduled, t_release: float) -> None:
        """Re-schedule a held frame into the slot right after `t_release` (its successor's)."""
        held.cancelled = True
        self._schedule(held.frame, t_release, held.t_arrival)  # same time, later seq: goes out second

    def _corrupt(self, frame: Frame, crc_only: bool) -> Frame:
        data = bytearray(frame.data)
        index = 0 if crc_only else self._rng.randrange(len(data))
        data[index] ^= 1 << self._rng.randrange(8)
        return Frame(frame.can_id, bytes(data))

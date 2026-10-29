"""The network as the host tools see it: the DBC, the protection scheme, decoding helpers.

Everything here derives from can/network.dbc (the one source of truth) and
can/e2e_ref.py (the Python reference of the CRC-8 / rolling-counter scheme), so
the host tools judge every frame by the same rules as the firmware's receivers.
"""

from __future__ import annotations

import dataclasses
import math
import pathlib
import sys

import cantools
from cantools.database.errors import DecodeError
from cantools.database.namedsignalvalue import NamedSignalValue

REPO = pathlib.Path(__file__).resolve().parents[1]
DBC_PATH = REPO / "can" / "network.dbc"

# can/ holds plain modules, not a package. It is appended to the search path,
# never put in front of it, so nothing in it can shadow python-can's `can`
# package or the standard library.
_CAN_DIR = str(REPO / "can")
if _CAN_DIR not in sys.path:
    sys.path.append(_CAN_DIR)
import e2e_ref  # noqa: E402

SIMULATED_BUS_NOTE = ("measured on a simulated bus (Linux vcan) on a desktop OS, "
                      "not on real-time hardware")


def attribute(db, obj, name: str, default=None):
    """A DBC attribute of a message or signal, falling back to its declared default."""
    attrs = obj.dbc.attributes if obj.dbc is not None else {}
    if name in attrs:
        return attrs[name].value
    definitions = db.dbc.attribute_definitions if db.dbc is not None else {}
    if name in definitions:
        return definitions[name].default_value
    return default


def is_yes(db, obj, name: str) -> bool:
    # ENUM attributes are exposed as the index into their choice list ("No", "Yes").
    return str(attribute(db, obj, name, 0)) in ("1", "Yes")


def choice_value(signal, name: str) -> int:
    """The raw value of a named choice of an enumerated signal (e.g. target_node "ALL" -> 0)."""
    for value, choice in (signal.choices or {}).items():
        if str(choice) == name:
            return int(value)
    raise KeyError(f"signal {signal.name} has no choice {name!r}")


def decimals(signal) -> int:
    """Decimal places that show one raw step of a scaled signal (scale 0.01 -> 2)."""
    scale = signal.scale if signal.scale else 1
    if float(scale).is_integer():
        return 0
    return max(0, -math.floor(math.log10(abs(scale))))


def format_value(signal, value) -> str:
    if isinstance(value, float):
        return f"{value:.{decimals(signal)}f}"
    return str(value)


@dataclasses.dataclass(frozen=True)
class Verdict:
    """What a receiver makes of one frame under the protection scheme."""

    protected: bool
    dlc_ok: bool
    crc_ok: bool | None = None          # None: not a protected message
    counter: int | None = None
    counter_check: str | None = None    # ok / repeated / skipped / out_of_sequence; CRC-valid frames only
    lost: int = 0                       # frames the counter says were missed before this one (skipped)

    @property
    def accepted(self) -> bool:
        """Right length and, for a protected message, a valid CRC and a counter that is
        ok or skipped (accepted with loss)."""
        if not self.dlc_ok:
            return False
        if not self.protected:
            return True
        return bool(self.crc_ok) and self.counter_check in ("ok", "skipped")

    @property
    def fault(self) -> str | None:
        """The fault a receiver would count: dlc, crc, repeated or out_of_sequence; else None."""
        if not self.dlc_ok:
            return "dlc"
        if self.protected and not self.crc_ok:
            return "crc"
        if self.counter_check in ("repeated", "out_of_sequence"):
            return self.counter_check
        return None

    def short(self) -> str:
        if not self.dlc_ok:
            return "dlc BAD"
        if not self.protected:
            return "-"
        if not self.crc_ok:
            return "crc BAD"
        text = f"ctr {self.counter:2d} {self.counter_check}"
        if self.lost:
            text += f" (lost {self.lost})"
        return text


class Network:
    """The DBC with the lookups the tools need."""

    def __init__(self, dbc_path: pathlib.Path = DBC_PATH) -> None:
        self.path = pathlib.Path(dbc_path)
        self.db = cantools.database.load_file(self.path, strict=True)
        self.by_id = {m.frame_id: m for m in self.db.messages}
        self.by_name = {m.name: m for m in self.db.messages}
        self.protected_ids = frozenset(m.frame_id for m in self.db.messages
                                       if is_yes(self.db, m, "SafetyRelevant"))
        self.signal_columns = [f"{m.name}.{s.name}" for m in self.db.messages for s in m.signals]

    def message(self, can_id: int):
        return self.by_id.get(can_id)

    def name(self, can_id: int) -> str:
        msg = self.by_id.get(can_id)
        return msg.name if msg is not None else f"0x{can_id:03X}"

    def lookup(self, name_or_id: str) -> int:
        """A frame identifier from a message name or a number ("SENSOR_DATA", "0x100", "256")."""
        if name_or_id in self.by_name:
            return self.by_name[name_or_id].frame_id
        return int(name_or_id, 0)

    def periodic(self) -> list:
        """Messages with a GenMsgCycleTime, in DBC order."""
        return [m for m in self.db.messages if m.cycle_time]

    def cycle_ms(self, can_id: int) -> int | None:
        msg = self.by_id.get(can_id)
        return msg.cycle_time if msg is not None and msg.cycle_time else None

    def decode(self, msg, data: bytes) -> dict[str, object]:
        """Physical values; enumerations as their names; {} for a frame of the wrong length."""
        try:
            values = msg.decode(bytes(data), decode_choices=True, scaling=True)
        except (DecodeError, ValueError):
            return {}
        return {k: (str(v) if isinstance(v, NamedSignalValue) else v) for k, v in values.items()}

    @staticmethod
    def format_values(msg, values: dict, skip: tuple[str, ...] = ("crc", "counter")) -> str:
        """Compact "name=value unit" text in DBC signal order; crc and counter are
        reported through the Verdict instead."""
        parts = []
        for signal in msg.signals:
            if signal.name in skip or signal.name not in values:
                continue
            text = f"{signal.name}={format_value(signal, values[signal.name])}"
            if signal.unit:
                text += f" {signal.unit}"
            parts.append(text)
        return " ".join(parts)


class E2EChecker:
    """Per-identifier CRC and rolling-counter check with the receiver's policy."""

    def __init__(self, network: Network) -> None:
        self.net = network
        self.trackers: dict[int, e2e_ref.CounterTracker] = {}

    def check(self, can_id: int, data: bytes) -> Verdict:
        msg = self.net.message(can_id)
        dlc_ok = msg is None or len(data) == msg.length
        if msg is None or can_id not in self.net.protected_ids:
            return Verdict(protected=False, dlc_ok=dlc_ok)
        crc = bool(dlc_ok and e2e_ref.crc_ok(can_id, data, msg.length))
        counter = e2e_ref.counter_of(data) if len(data) >= 2 else None
        check, lost = None, 0
        if crc:
            # A receiver evaluates the counter only on frames whose CRC is valid.
            tracker = self.trackers.setdefault(can_id, e2e_ref.CounterTracker())
            before = tracker.lost_total
            check = tracker.check(counter)
            lost = tracker.lost_total - before
        return Verdict(True, dlc_ok, crc, counter, check, lost)

    def reset(self) -> None:
        self.trackers.clear()

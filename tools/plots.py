#!/usr/bin/env python3
"""Draw the figures in docs/figures from recorded runs (matplotlib, SVG).

    python3 tools/plots.py [--results build/test-results] [--results-rust build/test-results-rust]
                           [--recording build/network-logs/record-and-analyze/bus.log]
                           [--demo build/demo/bus.log] [--out docs/figures]

Four figures, each drawn from a candump capture a run left behind (kernel
receive timestamps of the vcan interface), with the capture named in its footer:

  period-histograms.svg   SENSOR_DATA (10 ms) period histograms of the C++ and
                          the Rust sensor and CONTROLLER_COMMAND (20 ms) of the
                          C++ controller, from the system suite's periodic-timing
                          module (bus0.log of each results directory);
  echo-rtt.svg            echo round-trip distribution, host -> ECU -> host, per
                          responder, from the host tools' recorded run (the
                          ECHO_REQUEST and response frames matched by sequence);
  fault-timeline.svg      the heartbeat-loss scenario of tools/demo.py: the
                          sensor's heartbeat stops, the controller's command goes
                          SAFE_STATE after the timeout and NORMAL after the hold;
  watchdog-timeline.svg   the stalled-thread scenario of the suite's heartbeat
                          loss module, C++ and Rust sensor: the silence, the
                          reboot, the controller's safe state and recovery.

A figure whose input is missing is skipped with a note. Every number is from a
simulated bus (Linux vcan) on a desktop OS, not from real-time hardware; the
footers say which capture. Runs inside the dev container (matplotlib there).
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import statistics
import sys

import cantools
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parents[1]
if str(REPO / "tools") not in sys.path:
    sys.path.insert(0, str(REPO / "tools"))
import ecu_network as net  # noqa: E402  (read_frames: candump -L logs)

plt.rcParams.update({
    "svg.fonttype": "none",     # text stays text: small files, the viewer's fonts
    "svg.hashsalt": "can-ecu-sim",
    "figure.dpi": 100,
    "font.size": 9,
    "axes.titlesize": 10,
    "axes.spines.top": False,
    "axes.spines.right": False,
})
CPP = "#1f77b4"
RUST = "#d62728"
GREY = "#777777"
NOTE = "simulated bus (Linux vcan) on a desktop OS"
PNG_DIR: pathlib.Path | None = None  # --png: a PNG copy of each figure, for viewers without SVG


def save(fig, out: pathlib.Path, name: str) -> None:
    fig.savefig(out / f"{name}.svg")
    if PNG_DIR is not None:
        PNG_DIR.mkdir(parents=True, exist_ok=True)
        fig.savefig(PNG_DIR / f"{name}.png", dpi=110)


def rel(path: pathlib.Path) -> str:
    path = path.resolve()
    return str(path.relative_to(REPO)) if path.is_relative_to(REPO) else str(path)


def footer(fig, path: pathlib.Path, frames) -> None:
    when = dt.datetime.fromtimestamp(frames[0][0], dt.timezone.utc).strftime("%Y-%m-%d %H:%M UTC") if frames else "?"
    fig.text(0.01, 0.01, f"{rel(path)}, {len(frames)} frames, recorded {when}; {NOTE}", fontsize=7, color=GREY)


def periods_ms(frames, can_id: int) -> list[float]:
    ts = [t for t, i, _ in frames if i == can_id]
    return [(b - a) * 1000.0 for a, b in zip(ts, ts[1:])]


def percentile(values: list[float], p: float) -> float:
    s = sorted(values)
    k = max(0, min(len(s) - 1, round(p / 100.0 * (len(s) - 1))))
    return s[k]


def load(path: pathlib.Path | None):
    if path is None or not path.exists():
        return None
    frames = net.read_frames(path)
    return frames if frames else None


# --- 1. period histograms ------------------------------------------------------------------

def fig_periods(db, results: pathlib.Path, results_rust: pathlib.Path, out: pathlib.Path) -> bool:
    sd = db.get_message_by_name("SENSOR_DATA").frame_id
    cc = db.get_message_by_name("CONTROLLER_COMMAND").frame_id
    cpp_path = results / "system" / "test_periodic_timing" / "bus0.log"
    rust_path = results_rust / "system" / "test_periodic_timing" / "bus0.log"
    cpp = load(cpp_path)
    rust = load(rust_path)
    if cpp is None:
        print(f"period-histograms: no capture at {rel(cpp_path)}; skipped")
        return False
    panels = [("SENSOR_DATA, C++ sensor (Zephyr native_sim)", periods_ms(cpp, sd), 10.0, CPP, cpp_path, cpp)]
    if rust is not None:
        panels.append(("SENSOR_DATA, Rust sensor (sensor-vcan)", periods_ms(rust, sd), 10.0, RUST, rust_path, rust))
    panels.append(("CONTROLLER_COMMAND, C++ controller", periods_ms(cpp, cc), 20.0, CPP, cpp_path, cpp))
    fig, axes = plt.subplots(1, len(panels), figsize=(3.6 * len(panels), 3.4), sharey=False)
    axes = list(axes) if len(panels) > 1 else [axes]
    for ax, (title, values, nominal, color, _, _) in zip(axes, panels):
        lo, hi = nominal - 1.0, nominal + 1.0
        clipped = [min(max(v, lo), hi) for v in values]
        ax.hist(clipped, bins=80, range=(lo, hi), color=color, alpha=0.85)
        ax.axvline(nominal, color=GREY, linestyle="--", linewidth=0.8)
        ax.set_title(title)
        ax.set_xlabel("period (ms)")
        ax.set_ylabel("frames")
        ax.set_yscale("log")
        mean = statistics.fmean(values)
        std = statistics.pstdev(values)
        ax.text(0.02, 0.97, f"n = {len(values)}\nmean {mean:.3f} ms\nstd {std:.3f} ms\np99 {percentile(values, 99):.2f} ms\n"
                            f"min {min(values):.2f}, max {max(values):.2f} ms",
                transform=ax.transAxes, va="top", fontsize=8,
                bbox=dict(boxstyle="round", facecolor="white", edgecolor=GREY, alpha=0.9))
    fig.suptitle("Transmit periods on the bus under the system suite (nominal period dashed; bar at the edge = clipped)", fontsize=10)
    fig.tight_layout(rect=(0, 0.04, 1, 0.95))
    srcs = ", ".join(sorted({rel(p) for _, _, _, _, p, _ in panels}))
    fig.text(0.01, 0.01, f"{srcs}; {NOTE}", fontsize=7, color=GREY)
    save(fig, out, "period-histograms")
    plt.close(fig)
    print(f"period-histograms.svg: {', '.join(f'{t.split(',')[0]} n={len(v)}' for t, v, *_ in panels)}")
    return True


# --- 2. echo round trips --------------------------------------------------------------------

def fig_echo(db, recording: pathlib.Path, out: pathlib.Path) -> bool:
    frames = load(recording)
    if frames is None:
        print(f"echo-rtt: no recording at {rel(recording)}; skipped")
        return False
    req = db.get_message_by_name("ECHO_REQUEST")
    responders = {db.get_message_by_name("SENSOR_ECHO_RESPONSE").frame_id: "sensor ECU",
                  db.get_message_by_name("CONTROLLER_ECHO_RESPONSE").frame_id: "controller ECU"}
    resp_msgs = {i: db.get_message_by_frame_id(i) for i in responders}
    pending: dict[int, float] = {}
    rtts: dict[str, list[float]] = {name: [] for name in responders.values()}
    for t, i, d in frames:
        if i == req.frame_id:
            pending[int(req.decode(d, decode_choices=False)["seq"])] = t
        elif i in responders:
            seq = int(resp_msgs[i].decode(d, decode_choices=False)["seq"])
            if seq in pending:
                rtts[responders[i]].append((t - pending[seq]) * 1000.0)
    if not any(rtts.values()):
        print("echo-rtt: the recording holds no echo exchanges; skipped")
        return False
    fig, ax = plt.subplots(figsize=(7.2, 3.4))
    hi = max(percentile(v, 99.5) for v in rtts.values() if v) * 1.2
    for (name, values), color in zip(rtts.items(), (CPP, "#2ca02c")):
        if not values:
            continue
        ax.hist(values, bins=60, range=(0, hi), color=color, alpha=0.6,
                label=f"{name}: n = {len(values)}, p50 {percentile(values, 50):.2f} ms, p99 {percentile(values, 99):.2f} ms, max {max(values):.2f} ms")
    ax.set_xlabel("round trip, host -> ECU -> host (ms, kernel timestamps)")
    ax.set_ylabel("probes")
    ax.set_title("Echo round trips (host.echo: ECHO_REQUEST answered by each ECU's RX thread)")
    ax.legend(fontsize=8, loc="upper right")
    fig.tight_layout(rect=(0, 0.05, 1, 1))
    footer(fig, recording, frames)
    save(fig, out, "echo-rtt")
    plt.close(fig)
    print("echo-rtt.svg: " + "; ".join(f"{n} n={len(v)}" for n, v in rtts.items()))
    return True


# --- 3. the heartbeat-loss timeline (the demo) -----------------------------------------------

def mode_steps(db, frames, t0: float, t1: float):
    cmd = db.get_message_by_name("CONTROLLER_COMMAND")
    return [(t, str(cmd.decode(d, decode_choices=True)["mode"])) for t, i, d in frames
            if i == cmd.frame_id and t0 <= t <= t1]


def draw_timeline(ax, db, frames, t_ref: float, t0: float, t1: float, title: str, color: str) -> dict:
    """Ticks of the sensor's messages and the controller's command mode in [t0, t1], times relative to t_ref."""
    ids = {n: db.get_message_by_name(n).frame_id for n in ("SENSOR_DATA", "SENSOR_HEARTBEAT", "BATTERY_STATUS")}
    rows = [("SENSOR_DATA (10 ms)", ids["SENSOR_DATA"], 3.0), ("BATTERY_STATUS (100 ms)", ids["BATTERY_STATUS"], 2.0),
            ("SENSOR_HEARTBEAT (100 ms)", ids["SENSOR_HEARTBEAT"], 1.0)]
    for label, can_id, y in rows:
        ts = [t - t_ref for t, i, _ in frames if i == can_id and t0 <= t <= t1]
        ax.vlines(ts, y - 0.35, y + 0.35, color=color, linewidth=0.6 if y > 2.5 else 1.0)
        ax.text(t0 - t_ref, y, label + "  ", ha="right", va="center", fontsize=8)
    # The command's mode as a step line: SAFE_STATE drawn high, NORMAL low.
    steps = mode_steps(db, frames, t0, t1)
    ax.step([t - t_ref for t, _ in steps], [0.0 if m == "SAFE_STATE" else -0.9 for _, m in steps], where="post",
            color="#111111", linewidth=1.2)
    ax.text(t0 - t_ref, -0.9, "CONTROLLER_COMMAND mode: NORMAL  ", ha="right", va="center", fontsize=8)
    ax.text(t0 - t_ref, 0.0, "SAFE_STATE  ", ha="right", va="center", fontsize=8)
    ax.set_ylim(-1.5, 4.1)
    ax.set_yticks([])
    ax.set_xlim(t0 - t_ref, t1 - t_ref)
    ax.set_xlabel("time (s), kernel timestamps of the capture")
    ax.set_title(title)
    return steps


def fig_fault(db, demo: pathlib.Path, out: pathlib.Path) -> bool:
    frames = load(demo)
    if frames is None:
        print(f"fault-timeline: no demo capture at {rel(demo)}; skipped")
        return False
    tc = db.get_message_by_name("TEST_CONTROL")
    hb = db.get_message_by_name("SENSOR_HEARTBEAT").frame_id
    t_cmd = next((t for t, i, d in frames if i == tc.frame_id
                  and str(tc.decode(d, decode_choices=True)["command"]) == "SUPPRESS_HEARTBEAT"), None)
    if t_cmd is None:
        print("fault-timeline: no SUPPRESS_HEARTBEAT in the capture; skipped")
        return False
    t0, t1 = t_cmd - 0.4, t_cmd + 2.1
    hbs = [t for t, i, _ in frames if i == hb and t0 <= t <= t1]
    gaps = [(a, b) for a, b in zip(hbs, hbs[1:]) if b - a > 0.25]
    last_hb, back = gaps[0] if gaps else (None, None)
    steps = mode_steps(db, frames, t0, t1)
    t_safe = next((t for t, m in steps if last_hb and t > last_hb and m == "SAFE_STATE"), None)
    t_normal = next((t for t, m in steps if back and t > back and m == "NORMAL"), None)
    fig, ax = plt.subplots(figsize=(9.6, 3.8))
    draw_timeline(ax, db, frames, t_cmd, t0, t1, "Heartbeat loss: the sensor withholds SENSOR_HEARTBEAT for 600 ms (tools/demo.py)", CPP)
    ax.axvline(0.0, color=GREY, linestyle=":", linewidth=0.8)
    ax.text(0.0, 3.55, " TEST_CONTROL SUPPRESS_HEARTBEAT", fontsize=7.5, color=GREY, va="center")
    if last_hb and t_safe:
        ax.annotate("", xy=(t_safe - t_cmd, 0.5), xytext=(last_hb - t_cmd, 0.5), arrowprops=dict(arrowstyle="<->", color="#b22222", lw=1))
        ax.text(t_safe - t_cmd + 0.03, 0.28, f"SAFE_STATE {(t_safe - last_hb) * 1000:.0f} ms after the last heartbeat (timeout 350 ms)",
                ha="left", fontsize=8, color="#b22222")
    if back and t_normal:
        ax.annotate("", xy=(t_normal - t_cmd, -0.45), xytext=(back - t_cmd, -0.45), arrowprops=dict(arrowstyle="<->", color="#2e8b57", lw=1))
        ax.text((back + t_normal) / 2 - t_cmd, -1.3, f"NORMAL {(t_normal - back) * 1000:.0f} ms after the first heartbeat again (hold 500 ms)",
                ha="center", fontsize=8, color="#2e8b57")
    if last_hb and back:
        ax.text((last_hb + back) / 2 - t_cmd, 1.5, f"{(back - last_hb) * 1000:.0f} ms without a heartbeat", ha="center", fontsize=8)
    fig.tight_layout(rect=(0, 0.05, 1, 1))
    footer(fig, demo, frames)
    save(fig, out, "fault-timeline")
    plt.close(fig)
    print(f"fault-timeline.svg: safe {((t_safe - last_hb) * 1000 if last_hb and t_safe else float('nan')):.0f} ms, "
          f"normal {((t_normal - back) * 1000 if back and t_normal else float('nan')):.0f} ms")
    return True


# --- 4. the watchdog timeline, C++ and Rust -------------------------------------------------

def fig_watchdog(db, results: pathlib.Path, results_rust: pathlib.Path, out: pathlib.Path) -> bool:
    sd = db.get_message_by_name("SENSOR_DATA").frame_id
    tc = db.get_message_by_name("TEST_CONTROL")
    panels = []
    for label, d, color in (("C++ sensor (Zephyr native_sim, hardware-watchdog model)", results, CPP),
                            ("Rust sensor (sensor-vcan, software watchdog, re-exec)", results_rust, RUST)):
        bus0 = load(d / "system" / "test_heartbeat_loss" / "bus0.log")
        bus1 = load(d / "system" / "test_heartbeat_loss" / "bus1.log")
        if bus0 is None or bus1 is None:
            print(f"watchdog-timeline: no heartbeat-loss capture under {rel(d)}; panel skipped")
            continue
        # The module also kills and restarts the sensor (a longer silence); the watchdog
        # scenario is the first SENSOR_DATA gap over a second after its STALL_THREAD command.
        t_stall = next((t for t, i, d in bus0 if i == tc.frame_id
                        and str(tc.decode(d, decode_choices=True)["command"]) == "STALL_THREAD"), None)
        ts = [t for t, i, _ in bus0 if i == sd and (t_stall is None or t > t_stall - 0.5)]
        gap = next(((b - a, a, b) for a, b in zip(ts, ts[1:]) if b - a > 1.0), None)
        if gap is None or t_stall is None:
            print(f"watchdog-timeline: no watchdog silence under {rel(d)}; panel skipped")
            continue
        panels.append((label, bus0, bus1, gap, color, d / "system" / "test_heartbeat_loss" / "bus0.log"))
    if not panels:
        return False
    fig, axes = plt.subplots(len(panels), 1, figsize=(9.6, 3.6 * len(panels)))
    axes = list(axes) if len(panels) > 1 else [axes]
    for ax, (label, bus0, bus1, (silence, last, first), color, path) in zip(axes, panels):
        t0, t1 = last - 0.4, first + 1.0
        draw_timeline(ax, db, bus0, last, t0, t1, f"Stalled TX thread, watchdog reset: {label}", color)
        steps = mode_steps(db, bus1, t0, t1)
        ax.step([t - last for t, _ in steps], [0.0 if m == "SAFE_STATE" else -0.9 for _, m in steps], where="post",
                color="#111111", linewidth=1.2)
        t_safe = next((t for t, m in steps if t > last and m == "SAFE_STATE"), None)
        t_normal = next((t for t, m in steps if t > first and m == "NORMAL"), None)
        ax.annotate("", xy=(first - last, 3.6), xytext=(0.0, 3.6), arrowprops=dict(arrowstyle="<->", color=color, lw=1))
        ax.text((first - last) / 2, 3.85, f"{silence * 1000:.0f} ms without SENSOR_DATA: stall, liveness window, watchdog, reboot",
                ha="center", fontsize=8, color=color)
        if t_safe:
            ax.text(t_safe - last, 0.15, f" SAFE_STATE {(t_safe - last) * 1000:.0f} ms after the last frame", fontsize=8, color="#b22222")
        if t_normal:
            ax.text(t_normal - last, -1.3, f"NORMAL {(t_normal - first) * 1000:.0f} ms after the first frame again ", ha="right", fontsize=8, color="#2e8b57")
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    srcs = ", ".join(rel(p) for *_, p in panels)
    fig.text(0.01, 0.01, f"{srcs}; {NOTE}", fontsize=7, color=GREY)
    save(fig, out, "watchdog-timeline")
    plt.close(fig)
    print("watchdog-timeline.svg: " + "; ".join(f"{lab.split(' (')[0]} silence {s * 1000:.0f} ms" for lab, _, _, (s, _, _), _, _ in panels))
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--results", type=pathlib.Path, default=REPO / "build" / "test-results")
    parser.add_argument("--results-rust", type=pathlib.Path, default=REPO / "build" / "test-results-rust")
    parser.add_argument("--recording", type=pathlib.Path, default=REPO / "build" / "network-logs" / "record-and-analyze" / "bus.log")
    parser.add_argument("--demo", type=pathlib.Path, default=REPO / "build" / "demo" / "bus.log")
    parser.add_argument("--out", type=pathlib.Path, default=REPO / "docs" / "figures")
    parser.add_argument("--png", type=pathlib.Path, default=None, help="also write PNG copies to this directory")
    args = parser.parse_args()
    global PNG_DIR
    PNG_DIR = args.png
    args.out.mkdir(parents=True, exist_ok=True)
    db = cantools.database.load_file(net.DBC)
    drawn = [fig_periods(db, args.results, args.results_rust, args.out),
             fig_echo(db, args.recording, args.out),
             fig_fault(db, args.demo, args.out),
             fig_watchdog(db, args.results, args.results_rust, args.out)]
    print(f"{sum(drawn)} of 4 figures written to {rel(args.out)}")
    return 0 if all(drawn) else 2


if __name__ == "__main__":
    sys.exit(main())

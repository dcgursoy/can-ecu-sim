# can-ecu-sim

A simulated two-ECU CAN network on Zephyr RTOS, written in C++17, with a Python
fault-injection gateway, host tooling, an automated pytest suite, CI that also
builds the firmware for an STM32 Nucleo-F446RE, and (Phase 7) one ECU ported to
Rust as a cross-language conformance check. Pure software: the ECUs are Zephyr
`native_sim` processes on a Linux virtual CAN bus (`vcan`) inside a Docker dev
container, so the whole network runs identically on any machine and in CI.

> Status: **Phase 4 of 8** (host tooling: logger, live decoder, analysis). See
> [DESIGN.md](DESIGN.md) for every design decision and the phase plan, and
> [RESULTS.md](RESULTS.md) for the raw data behind every number below.

## What is here after Phase 4

- `can/network.dbc` the network: sensor data (10 ms), battery status (100 ms),
  controller command (20 ms), a heartbeat per ECU, an echo request/response
  pair and a test-only control message. Safety-relevant messages carry a 4-bit
  rolling counter and a CRC-8 that also covers the identifier.
- `can/gen_codec.py` generates a C++17 header-only codec from the DBC
  (`firmware/lib/protocol/`); `can/e2e_ref.py` is the independent Python
  implementation of the protection scheme; `can/crc_hd.py` is the measurement
  behind the polynomial choice.
- `firmware/lib/ecu_core/` the platform-independent ECU logic: message timeout
  monitor, protected-message acceptance policy, CAN error state machine with
  bus-off backoff and attempt limit, node state machine (BOOT, RUNNING,
  DEGRADED, SAFE_STATE, FAULT), thread liveness, and an ISO 11898-1 error
  counter model for the simulator. 70 GoogleTests cover both libraries.
- `firmware/app/` the Zephyr application. One codebase, the ECU role chosen by
  Kconfig. Three threads (CAN receive, periodic transmit, supervisor) with
  justified priorities, a HAL for the CAN controller and the watchdog, a
  `TEST_CONTROL` handler behind a test-build Kconfig, an `ecu status` shell
  command, and a link-time guard that makes any dynamic allocation after
  start-up a fatal error. Builds for `native_sim` (SocketCAN) and for
  `nucleo_f446re` (bxCAN, IWDG) in both roles.
- `tools/` wrappers to run anything inside the image, `build-ecu.sh`,
  `check-no-heap.sh`, the smoke test, `ecu_network.py` (runs both ECUs on vcan,
  drives their shells, measures safe-state entry and recovery and the bus-off
  recovery through the HAL hook), and the Docker Desktop on Windows `vcan`
  fix (`tools/wsl2/`).
- `gateway/` the fault-injection gateway between `vcan0` and `vcan1`: a Python
  relay that drops, delays and jitters, duplicates, reorders, corrupts and
  floods, per direction and per identifier, from a seeded random generator,
  reconfigurable at run time over a JSON control channel. Its fault engine is
  pure and unit-tested with a fake clock (17 tests).
- Firmware test hooks behind `CONFIG_APP_TEST_HOOKS` (on for `native_sim`,
  off for the Nucleo): `TEST_CONTROL` can suppress the heartbeat or any
  message, corrupt the CRC, skip or freeze the rolling counter, and block a
  thread to prove the liveness monitor and the watchdog.
- `tools/fault_scenarios.py` runs fifteen scenarios against one live network
  (both ECUs, the gateway, `candump` on each segment) and judges each one from
  the ECUs' counters, the gateway's counters and the captured frames, with the
  expected outcome of random faults derived from the capture.
  [docs/fault-model.md](docs/fault-model.md) maps every simulated fault to the
  hardware failure it stands for and lists what is not reproduced.
- `host/` the host tools, on python-can and cantools, usable against any
  SocketCAN interface: `host.logger` records an interface to a candump-format
  log and a decoded CSV with one column per DBC signal; `host.decoder` shows
  the traffic decoded, as a stream or as a live table with per-message
  statistics; `host.echo` sends round-trip probes and times the ECUs' answers
  from kernel timestamps; `host.analysis` reports bus load at a chosen bit
  rate, period and jitter per message and echo round-trip percentiles from a
  recording. Every frame is judged by the firmware's own protection rules via
  the Python reference. 20 unit tests run the tools on python-can's in-process
  virtual interface, no `vcan` needed.
- `tools/record_and_analyze.py` runs the network through all four tools at
  once and checks the recording against the DBC and the ECUs' own counters.
  Its output is the raw data in [RESULTS.md](RESULTS.md).
- `.github/workflows/ci.yml` builds the image, checks the generated code,
  runs the Python and C++ unit tests with coverage, builds all four firmware
  images, checks them for allocators, and runs the smoke test, the three
  network demos, the fault scenarios and the recorded run with the host tools
  on vcan.

## Quick start

Prerequisites: Docker Desktop on Windows (WSL2 backend) or Docker Engine on
Linux, plus Git (on Windows, Git for Windows, whose Git Bash the scripts use).
Nothing else is installed on the host. macOS: the image and the firmware build,
but the simulated bus needs the `vcan` driver in Docker Desktop's VM kernel,
which has not been verified there; treat the steps that use the bus as
Windows/Linux only.

1. **Windows only:** give the Docker VM a `vcan` driver. The first run builds the
   module (about five minutes); it is cached afterwards. The module is lost when
   the VM reboots (`wsl --shutdown`, a Windows reboot), and the wrappers in step 2
   reinstall it automatically in about ten seconds, so this step is only needed
   once by hand.

   ```powershell
   tools\wsl2\install-vcan-module.cmd
   ```

   (Git Bash: `tools/wsl2/install-vcan-module.sh`. The `.cmd` wrappers run
   PowerShell with `-ExecutionPolicy Bypass`, so they work on a stock Windows
   install where `.ps1` files are blocked.) Linux hosts instead run
   `sudo modprobe vcan`. Details in
   [DESIGN.md §6](DESIGN.md#6-development-environment-docker-capabilities-and-vcan-on-wsl2).

2. **Build the dev image** (about ten minutes the first time):

   ```bash
   tools/dev.sh --build
   ```

   PowerShell or cmd: `tools\dev.cmd -Build`. VS Code users can instead open the
   folder and choose "Reopen in Container".

3. **Build the firmware**, one image per board and role (Windows:
   `tools\dev.cmd` with the same arguments):

   ```bash
   tools/dev.sh tools/build-ecu.sh native_sim sensor
   ```

   ```bash
   tools/dev.sh tools/build-ecu.sh native_sim controller
   ```

   ```bash
   tools/dev.sh tools/build-ecu.sh nucleo_f446re controller
   ```

4. **Run the network on the virtual bus.** Both ECUs for three seconds, every
   frame verified against the DBC and the Python reference, both shells queried:

   ```bash
   tools/dev.sh python3 tools/ecu_network.py run
   ```

   Kill the sensor and bring it back, and measure how fast the controller
   enters and leaves its safe state:

   ```bash
   tools/dev.sh python3 tools/ecu_network.py safe-state
   ```

   Drive the controller's CAN controller into error-passive and bus-off through
   the test hook and watch the backoff, the recovery and what the bus saw:

   ```bash
   tools/dev.sh python3 tools/ecu_network.py bus-off
   ```

   Put the fault-injection gateway between the ECUs (sensor on `vcan0`,
   controller on `vcan1`) and run the fifteen fault scenarios, from a 15 ms
   jittering relay to a flooded segment and a stalled firmware thread that
   the watchdog resets (about 90 s; `--list` names them, `--scenarios a,b`
   picks some, `--seed` changes the random faults):

   ```bash
   tools/dev.sh python3 tools/fault_scenarios.py
   ```

   The gateway also runs on its own, e.g. `python3 -m gateway.gateway --both
   '{"drop": 0.1, "delay_ms": 5}'` inside the container, and prints its
   counters when stopped.

5. **Watch, record and analyse the bus with the host tools.** The one-shot
   version starts both ECUs, records eight seconds with the logger while the
   live decoder watches, sends 200 echo probes to both ECUs and prints the
   analysis of the recording: bus load, period and jitter per message, echo
   round-trip percentiles (logs in `build/network-logs/record-and-analyze/`):

   ```bash
   tools/dev.sh python3 tools/record_and_analyze.py
   ```

   The tools also run one at a time against any SocketCAN interface. Inside
   one container (`tools/dev.sh` with no arguments opens a shell; start the
   ECUs as described below), for example:

   ```bash
   python3 -m host.decoder --iface vcan0 --mode table
   ```

   ```bash
   python3 -m host.logger --iface vcan0 --duration 10 --out build/host-logs
   ```

   ```bash
   python3 -m host.echo --iface vcan0 --count 200
   ```

   ```bash
   python3 -m host.analysis build/host-logs/bus.log
   ```

   `host.decoder --mode stream` prints every frame decoded, and `--file
   bus.log` decodes a recording instead of an interface.

6. **Run the unit tests:** the Python reference implementation, the gateway's
   fault engine and the host tools, then the C++ libraries with coverage
   (report in `build/host-tests/coverage.txt`):

   ```bash
   tools/dev.sh python3 -m pytest -q can/ gateway/ host/
   ```

   ```bash
   tools/dev.sh tools/run-host-tests.sh
   ```

   After editing `can/network.dbc`, regenerate the codec and the test vectors
   with `tools/dev.sh tools/gen-all.sh`; CI fails if the committed copies are
   stale.

To poke around interactively, `tools/dev.sh` with no arguments opens a shell in
the container; inside it `tools/vcan-up.sh` creates `vcan0` and `vcan1`, then
`build/native_sim_sensor/zephyr/zephyr.exe --can-if=vcan0 &` and
`build/native_sim_controller/zephyr/zephyr.exe --can-if=vcan0` start the ECUs
(type `ecu status` at the prompt), and `candump vcan0` shows the traffic. Each
`tools/dev.sh <cmd>` call starts a fresh container with its own network
namespace, so interfaces created by one call are gone in the next: do
multi-process work inside one interactive shell or in the VS Code dev
container, whose `postStartCommand` runs `vcan-up.sh`.

## Layout

```
.devcontainer/   toolchain image            firmware/app/        Zephyr application (C++17)
.github/         CI                         firmware/lib/        protocol codec + ECU logic, host-tested
can/             network.dbc, generators,   gateway/             fault-injection gateway (Phase 3)
                 Python reference           host/                logger, decoder, echo probe, analysis
tools/           wrappers, vcan, demos      tests/               pytest suite (Phase 5)
docs/            phase logs, tool versions  rust/                Rust sensor ECU (Phase 7)
RESULTS.md       raw data behind the numbers below
```

## Results

Every number is **measured on a simulated bus (Linux `vcan`) on a desktop OS,
not on real-time hardware**, and comes from the command in its row;
[RESULTS.md](RESULTS.md) has the full output. Two kinds of number need
telling apart: the firmware's own clocks (timeouts, recovery hold, watchdog)
are exact and repeatable, while frame jitter and round-trip latency measure the
host's scheduler and the simulator's 1 ms driver poll and vary from run to run
(a busier run is quoted in parentheses).

| What | Measured | Command | Raw data |
|---|---|---|---|
| Bus load of the periodic traffic at 500 kbit/s | 4.00 % (DBC nominal 4.00 %; 4.86 % with worst-case stuffing); 5.51 % with the echo probes, busiest 100 ms window 7.4 % | `tools/dev.sh python3 tools/record_and_analyze.py --seed 2` | [RESULTS.md §1](RESULTS.md#1-phase-4-recording-of-the-plain-bus-network-with-the-host-tools) |
| SENSOR_DATA period, nominal 10 ms | mean 10.001 ms, std 0.10 ms, p99 10.25 ms, worst 10.53 ms (std 0.43, worst 11.43) | same | same |
| CONTROLLER_COMMAND period, nominal 20 ms | mean 20.001 ms, std 0.08 ms, p99 20.26 ms, worst 20.32 ms (std 0.47, worst 21.35) | same | same |
| Echo round trip, host → ECU → host | sensor p50 0.54 ms, p99 1.16 ms; controller p50 0.58 ms, p99 1.20 ms; 200 of 200 answered by each (p50 0.71 / 0.72, p99 2.2) | same | same |
| Safe state when the sensor heartbeat is lost | SAFE_STATE 374.7 ms after the last heartbeat (timeout 350 ms); NORMAL 514.6 ms after the first heartbeat again (hold 500 ms) | `tools/dev.sh python3 tools/fault_scenarios.py --scenarios drop-heartbeat` | [phase3.md](docs/phase-logs/phase3.md) |
| Safe state when the sensor process dies | SAFE_STATE 54.1 ms after the last SENSOR_DATA (timeout 35 ms); NORMAL 525 ms after the restarted sensor's first heartbeat | `tools/dev.sh python3 tools/ecu_network.py safe-state` | [phase2.md](docs/phase-logs/phase2.md) |
| Bus-off recovery of the simulated controller | recovered in 103 ms; one 120 ms command gap on the bus | `tools/dev.sh python3 tools/ecu_network.py bus-off` | [phase2.md](docs/phase-logs/phase2.md) |
| Stalled transmit thread | watchdog reset after 1313 ms of silence (300 ms liveness window + 1000 ms timeout) | `tools/dev.sh python3 tools/fault_scenarios.py --scenarios hook-stall-thread` | [phase3.md](docs/phase-logs/phase3.md) |
| Gateway relay latency, no faults | median 0.13 ms, p99 0.27 ms (Python relay) | `tools/dev.sh python3 tools/fault_scenarios.py --scenarios baseline` | [phase3.md](docs/phase-logs/phase3.md) |
| Random loss, 10 % both ways for 3 s | all 50 lost frames accounted for by the receivers' counters | `tools/dev.sh python3 tools/fault_scenarios.py --scenarios drop-random` | [phase3.md](docs/phase-logs/phase3.md) |

## License

MIT, see [LICENSE](LICENSE).

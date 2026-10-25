# can-ecu-sim

A simulated two-ECU CAN network on Zephyr RTOS, written in C++17, with a Python
fault-injection gateway, host tooling, an automated pytest suite, CI that also
builds the firmware for an STM32 Nucleo-F446RE, and (Phase 7) one ECU ported to
Rust as a cross-language conformance check. Pure software: the ECUs are Zephyr
`native_sim` processes on a Linux virtual CAN bus (`vcan`) inside a Docker dev
container, so the whole network runs identically on any machine and in CI.

> Status: **Phase 1 of 8** (network definition and codec). See
> [DESIGN.md](DESIGN.md) for every design decision and the phase plan.

## What is here after Phase 1

- `can/network.dbc` the network: sensor data (10 ms), battery status (100 ms),
  controller command (20 ms), a heartbeat per ECU, an echo request/response
  pair and a test-only control message. Safety-relevant messages carry a 4-bit
  rolling counter and a CRC-8 that also covers the identifier.
- `can/gen_codec.py` generates a C++17 header-only codec from the DBC
  (`firmware/lib/protocol/`), `can/e2e_ref.py` is the independent Python
  implementation of the protection scheme, and `can/crc_hd.py` is the
  measurement behind the polynomial choice.
- `firmware/lib/protocol/tests/` 34 GoogleTests cross-check the C++ codec
  against cantools and the Python reference, with line and function coverage of
  the library reported by gcovr.
- `.devcontainer/` one Docker image with the Zephyr SDK, a pinned Zephyr v4.3.1
  west workspace, can-utils, GoogleTest, Python host tooling and Rust.
- `firmware/app/` a minimal Zephyr C++17 application that sends the protected
  sensor heartbeat through the Zephyr CAN API; it builds for `native_sim`
  (SocketCAN) and for `nucleo_f446re` (on-chip bxCAN) from the same source.
- `tools/` wrappers to run anything inside the image, a vcan setup script, a
  smoke test that verifies the live frames with the Python reference, and a fix
  for Docker Desktop on Windows whose WSL2 kernel lacks the `vcan` driver
  (`tools/wsl2/`).
- `.github/workflows/ci.yml` builds the image, checks the generated code is
  current, runs the Python and C++ unit tests with coverage, builds both
  firmware targets, and runs the smoke test on vcan.

## Quick start

Prerequisites: Docker Desktop on Windows (WSL2 backend) or Docker Engine on
Linux, plus Git (on Windows, Git for Windows, whose Git Bash the scripts use).
Nothing else is installed on the host. macOS: the image and both firmware
targets build, but the simulated bus needs the `vcan` driver in Docker Desktop's
VM kernel, which has not been verified there; treat step 4 (the live-bus smoke
test) as Windows/Linux only. Steps 2, 3 and 5 need no `vcan`.

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

3. **Build the firmware for the simulator and for the STM32 board:**

   ```bash
   tools/dev.sh west build -p always -b native_sim -s firmware/app -d build/native_sim
   ```

   ```bash
   tools/dev.sh west build -p always -b nucleo_f446re -s firmware/app -d build/nucleo_f446re
   ```

   (Windows: `tools\dev.cmd west build ...` with the same arguments.)

4. **Watch the simulated ECU talk on the virtual bus:**

   ```bash
   tools/dev.sh tools/smoke-test.sh
   ```

   This creates `vcan0`, starts `build/native_sim/zephyr/zephyr.exe --can-if=vcan0`,
   runs `candump` until five `SENSOR_HEARTBEAT` frames (ID `0x200`) have been
   seen, and verifies each one (decode, CRC, counter) with the Python reference.

5. **Run the unit tests:** the Python reference implementation, then the C++
   protocol library with coverage (report in `build/host-tests/coverage.txt`):

   ```bash
   tools/dev.sh python3 -m pytest -q can/
   ```

   ```bash
   tools/dev.sh tools/run-host-tests.sh
   ```

   After editing `can/network.dbc`, regenerate the codec and the test vectors
   with `tools/dev.sh tools/gen-all.sh`; CI fails if the committed copies are
   stale.

To poke around interactively, `tools/dev.sh` with no arguments opens a shell in
the container; inside it `tools/vcan-up.sh` creates `vcan0` and `vcan1`, then
`candump vcan0` and `cansend vcan0 123#DEADBEEF` work as on any Linux CAN setup.
Each `tools/dev.sh <cmd>` call starts a fresh container with its own network
namespace, so interfaces created by one call are gone in the next: do
multi-process work (two ECUs, candump, the gateway) inside one interactive shell
or in the VS Code dev container, whose `postStartCommand` runs `vcan-up.sh`.

## Layout

```
.devcontainer/   toolchain image            firmware/   Zephyr C++17 application
.github/         CI                         gateway/    fault-injection gateway (Phase 3)
can/             network.dbc (Phase 1)      host/       logger, decoder, analysis (Phase 4)
tools/           wrappers, vcan, smoke test tests/      pytest suite (Phase 5)
docs/            phase logs, tool versions, rust/       Rust sensor ECU (Phase 7)
                 fault-model mapping (Phase 3)
```

## Results

Filled in from real runs starting with Phase 4. Every number will be labelled as
measured on a simulated bus on a desktop OS and will cite the command that
produced it; raw data goes to `RESULTS.md`.

## License

MIT, see [LICENSE](LICENSE).

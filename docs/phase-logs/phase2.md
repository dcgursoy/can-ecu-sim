# Phase 2 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2).
Every command ran through `tools/dev.sh` inside the dev image, after the
adversarial review of this phase (41 findings, all fixed; see section 7). All
timing numbers are host-side timestamps of a simulated bus on a desktop OS, not
real-time hardware.

## 1. Host unit tests (both libraries)

```
$ tools/run-host-tests.sh build/host-tests
100% tests passed, 0 tests failed out of 70
lines: 94.7% (990 out of 1045)
functions: 94.1% (160 out of 170)
branches: 80.9% (487 out of 602)
```

34 tests for the protocol library (Phase 1) and 36 for `ecu_core`: timeout
monitor, protected receiver policy, error-counter model, CAN error state
machine (backoff doubling and cap, attempt limit, stable-period reset,
refused requests, polled recovery completion, a recovery that does not hold,
reset while still bus-off), node state machine (boot, safe-state entry and
hold, degraded, fault latch and reset, sensor policy) and liveness. Coverage is
measured with `-fkeep-inline-functions`.

## 2. Firmware builds, four images from one source tree

```
$ tools/build-ecu.sh native_sim sensor        (clean)
$ tools/build-ecu.sh native_sim controller    (clean)
$ tools/build-ecu.sh nucleo_f446re sensor
           FLASH:       83916 B       512 KB     16.01%
             RAM:       23680 B       128 KB     18.07%
$ tools/build-ecu.sh nucleo_f446re controller
           FLASH:       79984 B       512 KB     15.26%
             RAM:       23808 B       128 KB     18.16%
```

The Nucleo images contain the shell, deferred logging, libstdc++ pieces, the
IWDG driver and the FPU context switching; the application itself is a small
part of it.

## 3. No dynamic allocation after initialisation

```
$ tools/check-no-heap.sh build/nucleo_f446re_controller
== allocator call sites in build/nucleo_f446re_controller
  (… objects scanned)
  zephyr/kernel/CMakeFiles/kernel.dir/kheap.c.obj:         U sys_heap_aligned_alloc
  zephyr/kernel/CMakeFiles/kernel.dir/mempool.c.obj:         U sys_heap_aligned_alloc
  zephyr/lib/libc/picolibc/CMakeFiles/lib__libc__picolibc.dir/locks.c.obj:         U malloc
== final image zephyr/zephyr.elf
  k_heap_alloc: not in the image (no reachable call path, dead-stripped)
  … (every wrapped allocator: not in the image)
OK: no allocator call sites outside the allowlist; every surviving allocator is guarded
```

Same result on all four images. The first build flagged a real caller: the
shell's command history allocates from a `k_heap` on every command
(`shell_history.c`). It is disabled; after that the linker removed every
allocator from the image because nothing reachable calls one. The link-time
guard (`src/alloc_guard.c`) is still armed for any future regression.

## 4. The network on vcan

```
$ python3 tools/ecu_network.py run --duration 3
captured 542 frames over 3.00 s on vcan0
  BATTERY_STATUS            30 frames    10.0 /s  (expected 10)
  CONTROLLER_COMMAND       151 frames    50.0 /s  (expected 50)
  CONTROLLER_HEARTBEAT      30 frames    10.0 /s  (expected 10)
  SENSOR_DATA              301 frames   100.1 /s  (expected 100)
  SENSOR_HEARTBEAT          30 frames    10.0 /s  (expected 10)
verified 542 frames (542 protected, CRC-checked): 0 wrong DLC, 0 bad CRC, 0 counter faults -> OK
--- sensor `ecu status`
  node: state=RUNNING reason=inputs established since_ms=2925 safe_state_entries=0 error_flags=0x00
  rx_summary: frames=180 unknown=0 queue_hwm=2/32 dropped=0 events_dropped=0 echo_responses=0 test_commands=0
--- controller `ecu status`
  node: state=RUNNING reason=inputs established since_ms=2935 safe_state_entries=0 error_flags=0x00
  rx_summary: frames=360 unknown=0 queue_hwm=2/32 dropped=0 events_dropped=0 echo_responses=0 test_commands=0
RESULT: OK
```

Rates are measured over each message's own first-to-last frame; the receive
queue never held more than two frames; no frame was dropped; the watchdog was
fed on every 5 ms supervisor tick (`wdt: … skipped=0`).

## 5. Safe state when the sensor dies

Sensor process killed with SIGKILL two seconds in, restarted two seconds later.

```
$ python3 tools/ecu_network.py safe-state --kill-at 2 --restart-at 4 --duration 7
last SENSOR_DATA before kill      : 1792983971.736392
last SENSOR_HEARTBEAT before kill : 1792983971.716521
first CONTROLLER_COMMAND SAFE_STATE: 1792983971.790468
detection latency from last SENSOR_DATA      : 54.1 ms (timeout 35 ms + supervisor tick + command period)
detection latency from last SENSOR_HEARTBEAT : 73.9 ms
first sensor frame after restart   : 1792983973.765206
first SENSOR_HEARTBEAT after restart: 1792983973.825649 (+60.4 ms; both critical inputs alive from here)
first CONTROLLER_COMMAND NORMAL    : 1792983974.350610
recovery time from first sensor frame   : 585.4 ms (wait for the first heartbeat + hold 500 ms + supervisor tick + command period)
recovery time from first SENSOR_HEARTBEAT: 525.0 ms (hold 500 ms + supervisor tick + command period)
--- controller log transitions (firmware uptime)
  [00:00:00.080,000] <inf> ecu: node_state: BOOT -> RUNNING (inputs established)
  [00:00:02.035,000] <wrn> ecu: timeout: SENSOR_DATA not received for 35 ms
  [00:00:02.035,000] <inf> ecu: node_state: RUNNING -> SAFE_STATE (critical input lost)
  [00:00:02.035,000] <wrn> role_controller: safe state: actuators disabled, brake hold, speed limit 0
  [00:00:02.300,000] <wrn> ecu: timeout: BATTERY_STATUS not received for 350 ms
  [00:00:02.330,000] <wrn> ecu: timeout: SENSOR_HEARTBEAT not received for 350 ms
  [00:00:04.590,000] <inf> ecu: node_state: SAFE_STATE -> RUNNING (inputs healthy again)
RESULT: OK
```

Killing the whole sensor process stops all its messages, so the 35 ms sensor
data timeout fires first and the heartbeat timeout 295 ms later (the last
heartbeat preceded the last data frame by 20 ms, so 350 - 20 - 35), by then
already in the safe state. Suppressing only the heartbeat (the pure "heartbeat
loss" case) is a Phase 3 test hook. The recovery figure the policy bounds is
the one measured from the restarted sensor's first heartbeat (525.0 ms: 500 ms
hold plus up to one supervisor tick and one command period); the 60 ms before
it is the restarted sensor's heartbeat slot.

## 6. Error-passive and bus-off through the HAL test hook

```
$ python3 tools/ecu_network.py bus-off
  [00:00:01.500,400] <inf> test_ctrl: TEST_CONTROL INJECT_CAN_ERROR x30 -> ERROR_PASSIVE tec=240 rec=0
  [00:00:01.505,000] <wrn> ecu: can: ERROR_ACTIVE -> ERROR_PASSIVE (tec=240 rec=0) phase active -> passive
  [00:00:03.375,000] <wrn> ecu: can: ERROR_PASSIVE -> ERROR_ACTIVE (tec=127 rec=0) phase passive -> active
  [00:00:04.001,800] <inf> test_ctrl: TEST_CONTROL FORCE_BUS_OFF -> BUS_OFF
  [00:00:04.005,000] <wrn> ecu: can: ERROR_ACTIVE -> BUS_OFF (tec=255 rec=0) phase active -> bus_off_backoff
  [00:00:04.005,000] <inf> ecu: node_state: RUNNING -> SAFE_STATE (bus off)
  [00:00:04.100,000] <wrn> ecu: bus-off recovery attempt 1 after 100 ms backoff: requested (0)
  [00:00:04.105,000] <wrn> ecu: can: BUS_OFF -> ERROR_ACTIVE (tec=0 rec=0) phase bus_off_recovering -> active
  [00:00:04.605,000] <inf> ecu: node_state: SAFE_STATE -> RUNNING (inputs healthy again)
  can: bus_off=1 passive=1 recovery_attempts=1 recoveries_ok=1 recovery_failures=0 last_recovery_ms=103 backoff_ms=100
CONTROLLER_COMMAND gap of 120.0 ms starting 1792983981.222848 (controller silent while bus-off)
CONTROLLER_HEARTBEAT can_state values seen on the bus: ['ERROR_ACTIVE', 'ERROR_PASSIVE']
ERROR_PASSIVE visible in 19 heartbeats, from 0.071 s to 1.871 s after the injection (TEC healing by one per successful frame)
RESULT: OK
```

The sensor's view of the same event: one `CONTROLLER_COMMAND` timeout (the
120 ms gap exceeds its 70 ms timeout) and one heartbeat counter gap
(`skipped=1 lost=1`), which is what a real bus-off of this length looks like to
a peer. Error-passive lasted 1.87 s: TEC 240 healed by one per successful frame
at about 60 frames/s until it dropped below 128. On the simulator the state
changes arrive as events from the simulated controller; on the Nucleo the
return to error-active would arrive through the supervisor's per-tick poll
(see section 7), which the simulator cannot exercise.

## 7. Defects found while verifying and in review

Found by running the network:

- **Back-to-back `can_send` with no mailbox wait.** The SocketCAN driver has a
  single TX mailbox that stays busy until the kernel echoes the frame back, so
  the second frame in a tick failed with `-EAGAIN`: 60 of 360 sends per three
  seconds, every battery status and heartbeat. Visible as `busy=60` in the shell
  and as a sensor node in DEGRADED. Fixed with a bounded 5 ms mailbox wait.
- **Watchdog warning at boot.** The supervisor's first tick ran before the TX
  thread's first check-in. Threads now check in before their first wait.
- **Safe state at boot.** RUNNING was entered before any input had arrived, so
  the controller was in SAFE_STATE 5 ms after start and counted it as an entry.
  The node now stays in BOOT until its inputs arrive or time out.
- **Host I/O stalls the simulator.** Console output is synchronous host I/O;
  with logs on the Windows bind mount a single `ecu status` print stalled the
  process long enough for the peer to see a 35 ms gap. Logs now stay inside the
  container during a run (DESIGN.md §11.7).
- **Shell history allocates.** Found by the allocator check; disabled.

Found by the adversarial review (41 confirmed findings, the ones that changed
behaviour):

- **bxCAN never reports the return to error-active.** Its status interrupt
  fires when an error flag is set, never when it clears, so the recovery
  handshake would have timed out five times and latched FAULT on a healthy
  bus. The supervisor now also polls the controller state every tick and feeds
  it to the state machine level-triggered (DESIGN.md §11.4). The simulator
  could never show this because its simulated controller reports both
  directions.
- **The Nucleo image could not boot.** The STM32 IWDG driver arms a 100 ms
  timeout at POST_KERNEL unless `CONFIG_WDT_DISABLE_AT_BOOT=y`, and then
  rejects the application's timeout; `app::start()` would have failed and the
  unfed watchdog reset the board forever. Set in the board conf, with a
  compile-time check in `hal/watchdog.cpp`.
- **A flapping node never escalated.** A recovery that held for milliseconds
  and dropped off again restarted from the first backoff forever; it now
  counts as a failed attempt. **`reset` while still bus-off** went to active
  without ever re-arming recovery; it now restarts the backoff cycle. **A
  repeated error-active report** (bxCAN's error-warning level, folded into
  active) restarted the 10 s stable window; now idempotent.
- **`reset` from FAULT** jumped straight to RUNNING; it now re-enters
  SAFE_STATE (or DEGRADED for the sensor) and leaves through the normal hold.
- **Formatting under the spinlock.** `ecu status` ran `snprintf` with
  interrupts masked; the roles now snapshot under the lock and format outside.
- **Simulated controller state read without the lock** by the TX, supervisor
  and shell threads; the HAL now hands out consistent snapshots.
- **Allocation guard gaps.** `k_malloc`, `k_calloc`, `k_realloc` and the
  realloc family bypass `k_heap_alloc` and were not wrapped; the checker's
  pattern missed the aligned and nothrow `operator new` overloads and was
  fail-open on a missing tool or image. Both fixed.
- **No receive-drop counter.** The driver dropped frames silently on a full
  queue; the HAL now owns the receive callback and counts drops (`dropped` in
  the shell).
- **native_sim watchdog expiry** only exited the process; with
  `CONFIG_NATIVE_SIM_REBOOT=y` it restarts it.
- **Tooling.** The network runner now waits for the shell's last output line
  instead of a fixed sleep, restarts the same process object, measures rates
  over the capture span, names every failed criterion, distinguishes a host
  scheduling stall (inconclusive, retried) from a real failure, and publishes
  the logs even when a demo raises. `build-ecu.sh` forwards CMake options.

## 8. Not demonstrated yet

The watchdog is armed and fed (`feeds` in the shell) but a reset was not
provoked: that needs a hook that stalls a thread, which belongs to the Phase 3
fault-injection set together with the remaining `TEST_CONTROL` commands
(heartbeat suppression, CRC corruption, counter faults, message suppression).
The polled recovery completion on bxCAN is covered by unit tests of the state
machine, not by a run on hardware.

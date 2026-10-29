# Phase 3 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2).
Every command ran through `tools/dev.sh` inside the dev image. Per the review
policy adopted for this phase, only the firmware hook changes went through the
adversarial review (one firmware lens, one verifier per finding; section 6);
the Python tooling and the documentation did not. All timing numbers are
host-side timestamps of a simulated bus on a desktop OS, not real-time
hardware.

## 1. Generated code and host tests after the DBC change

`can/network.dbc` gained the `STALL_THREAD` command value and the full
argument documentation of `TEST_CONTROL`; the codec and the test vectors were
regenerated.

```
$ tools/gen-all.sh --check
firmware/lib/protocol/include/protocol/messages.hpp: up to date
firmware/lib/protocol/tests/generated/test_vectors.hpp: up to date
$ tools/run-host-tests.sh build/host-tests
100% tests passed, 0 tests failed out of 70
lines: 95.1% (899 out of 945)
functions: 94.5% (156 out of 165)
```

The denominators differ from the Phase 2 log because gcovr now searches the
build directory only (`tools/run-host-tests.sh`); with `--root .` and no
search path it walked the whole checkout, including the four Zephyr build
trees on the bind mount, and took longer than ten minutes.

## 2. Gateway fault-engine unit tests

```
$ python3 -m pytest -q gateway/
17 passed in 0.91s
```

Fake-clock tests of `gateway/faults.py`: immediate in-order forwarding,
drop (total and per id), the id filter, exact fixed delay, jitter within bounds
with FIFO order preserved even when the jitter exceeds the inter-frame gap,
duplicate back to back, reorder as a swap with the successor (and no chaining),
release at the hold deadline without a successor, a dropped successor still
releasing the held frame, exactly one flipped bit (and CRC-byte-only),
seed reproducibility, probabilities near their targets over 5000 frames,
JSON round trip of the configuration, frame validation, and reconfiguration
mid-stream keeping the FIFO.

## 3. Firmware builds, four images

```
$ tools/build-ecu.sh native_sim sensor          (clean, no warnings)
$ tools/build-ecu.sh native_sim controller      (clean, no warnings)
$ tools/build-ecu.sh nucleo_f446re sensor
           FLASH:       84312 B       512 KB     16.08%
             RAM:       23680 B       128 KB     18.07%
$ tools/build-ecu.sh nucleo_f446re controller
           FLASH:       80284 B       512 KB     15.31%
             RAM:       23808 B       128 KB     18.16%
$ tools/check-no-heap.sh build/<each>
OK: no allocator call sites outside the allowlist; every surviving allocator is guarded   (x4)
```

The Nucleo images, built without `CONFIG_APP_TEST_HOOKS`, grew by 396 B
(sensor) and 300 B (controller) of flash over Phase 2: the four hook counters
in the status struct, the shell lines that print and describe them, and the
restructured transmit path that settles the applied faults; the hook logic
itself is compiled out. One run of the allocator check, started right after
the Nucleo sensor link, read the image back without its `main`/`z_cstart`
symbols and failed, as a fail-closed check should; the same file passed a
minute later and `nm` shows both symbols. The script now re-reads the image
for up to three seconds before failing, which covers a momentarily incoherent
file on Docker Desktop's bind mount without weakening the check.

*Corrected in Phase 4:* the file was never incoherent. The check piped the
symbol table into `grep -q` under `set -o pipefail`; `grep -q` exits at the
first match and the writer can die of SIGPIPE, so a healthy image fails the
test on a loaded host (reproduced 26 times in 2000 tries under CPU load, and
once more in the first CI run). The test now reads the whole symbol table
(`grep -c`) and the retry loop is gone; see
[phase4.md](phase4.md#6-the-first-ci-run-and-the-allocator-check).

## 4. Fault scenarios on the gateway topology, seed 1

Run on the firmware after the review fixes of section 6.

```
$ python3 tools/fault_scenarios.py --retries 2 --seed 1
topology: sensor@vcan0 <-> gateway (seed 1) <-> controller@vcan1; both RUNNING
=== baseline: all traffic through the relay: rates on both segments and the relay's own latency
  vcan0 BATTERY_STATUS          31 frames   10.0/s (expected 10)
  vcan0 CONTROLLER_COMMAND     152 frames   50.0/s (expected 50)
  vcan0 CONTROLLER_HEARTBEAT    30 frames   10.0/s (expected 10)
  vcan0 SENSOR_DATA            303 frames  100.1/s (expected 100)
  vcan0 SENSOR_HEARTBEAT        30 frames   10.0/s (expected 10)
  vcan1 BATTERY_STATUS          31 frames   10.0/s (expected 10)
  vcan1 CONTROLLER_COMMAND     152 frames   50.0/s (expected 50)
  vcan1 CONTROLLER_HEARTBEAT    30 frames   10.0/s (expected 10)
  vcan1 SENSOR_DATA            304 frames  100.1/s (expected 100)
  vcan1 SENSOR_HEARTBEAT        30 frames   10.0/s (expected 10)
  relay latency SENSOR_DATA vcan0->vcan1: median 0.126 ms, p99 0.267 ms, max 0.303 ms (n=303; Python relay on a desktop OS)
  relay latency CONTROLLER_COMMAND vcan1->vcan0: median 0.112 ms, p99 0.231 ms, max 0.247 ms (n=152; Python relay on a desktop OS)
  gateway a_to_b: received 363, forwarded 363, dropped 0, tx_errors 0
  gateway b_to_a: received 182, forwarded 182, dropped 0, tx_errors 0
  [ok] vcan0 BATTERY_STATUS rate within 10 %
  [ok] vcan0 CONTROLLER_COMMAND rate within 10 %
  [ok] vcan0 CONTROLLER_HEARTBEAT rate within 10 %
  [ok] vcan0 SENSOR_DATA rate within 10 %
  [ok] vcan0 SENSOR_HEARTBEAT rate within 10 %
  [ok] vcan1 BATTERY_STATUS rate within 10 %
  [ok] vcan1 CONTROLLER_COMMAND rate within 10 %
  [ok] vcan1 CONTROLLER_HEARTBEAT rate within 10 %
  [ok] vcan1 SENSOR_DATA rate within 10 %
  [ok] vcan1 SENSOR_HEARTBEAT rate within 10 %
  [ok] relay latency SENSOR_DATA vcan0->vcan1 median below 2 ms
  [ok] relay latency CONTROLLER_COMMAND vcan1->vcan0 median below 2 ms
  [ok] gateway a_to_b forwarded every frame
  [ok] gateway b_to_a forwarded every frame
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
  [ok] controller unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== delay-jitter: 15 +/- 5 ms latency in both directions: period-based timeouts do not see latency
  SENSOR_DATA latency vcan0->vcan1 with 15 +/- 5 ms configured: p1 10.2 ms, median 15.2 ms, p99 20.0 ms (n=285)
  gateway max scheduled latency: a->b 19.93 ms, b->a 19.99 ms
  vcan1 BATTERY_STATUS          29 frames   10.0/s (expected 10)
  vcan1 SENSOR_DATA            290 frames  100.2/s (expected 100)
  vcan1 SENSOR_HEARTBEAT        29 frames   10.0/s (expected 10)
  vcan0 CONTROLLER_COMMAND     145 frames   50.1/s (expected 50)
  vcan0 CONTROLLER_HEARTBEAT    29 frames   10.0/s (expected 10)
  [ok] median latency between 13 and 17 ms
  [ok] p1 above 9.5 ms and p99 below 23 ms
  [ok] gateway never scheduled more than 20 ms
  [ok] vcan1 BATTERY_STATUS rate within 10 %
  [ok] vcan1 SENSOR_DATA rate within 10 %
  [ok] vcan1 SENSOR_HEARTBEAT rate within 10 %
  [ok] vcan0 CONTROLLER_COMMAND rate within 10 %
  [ok] vcan0 CONTROLLER_HEARTBEAT rate within 10 %
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
  [ok] controller unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== duplicate: 20 % of sensor->controller frames delivered twice: every copy rejected as a repeated counter
  gateway duplicated 34 of 362 sensor->controller frames; controller rejected 34 frames as repeated counter
  [ok] controller counter rejections == duplicated frames
  [ok] COUNTER_FAULT_SEEN flag clear before and published in the controller heartbeat after
  [ok] controller: no CRC faults, timeouts or safe-state entries, RUNNING
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== reorder: 20 % of SENSOR_DATA swapped with their successor: one accepted with loss, one out of sequence
  gateway swapped 39 SENSOR_DATA frames with their successor (0 released late without one)
  controller SENSOR_DATA: 39 accepted with loss (39 frames counted lost), 39 rejected out of sequence
  [ok] out-of-sequence rejections == swaps
  [ok] accepted-with-loss events == swaps (the successor arrives with counter +2)
  [ok] controller: no CRC faults, timeouts or safe-state entries, RUNNING
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== corrupt: one bit flipped in 10 % of sensor->controller frames: every one rejected by the E2E CRC
  gateway corrupted 20 of 423 frames; controller rejected 20 by CRC
  SENSOR_DATA: 14 runs of consecutive corrupted frames, longest 2; 0 long enough for a timeout
  SENSOR_HEARTBEAT: 1 runs of consecutive corrupted frames, longest 1; 0 long enough for a timeout
  controller node transitions: none
  [ok] controller CRC rejections == corrupted frames
  [ok] SENSOR_DATA timeouts == runs longer than the timeout (0)
  [ok] SENSOR_HEARTBEAT timeouts == runs longer than the timeout (0)
  [ok] controller safe_state_entries == 0 (timeout runs)
  [ok] controller RUNNING again
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== corrupt-battery: every BATTERY_STATUS corrupted for 350 ms: three consecutive rejects degrade the controller
  4 consecutive BATTERY_STATUS frames corrupted; controller rejected 4 by CRC
  controller node transitions: [00:00:18.145,000] <inf> ecu: node_state: RUNNING -> DEGRADED (input quality degraded); [00:00:18.345,000] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal)
  [ok] CRC rejections == corrupted frames (3 or 4)
  [ok] controller DEGRADED on the third consecutive reject
  [ok] no safe state (battery is not a critical input); RUNNING again
  [ok] SENSOR_DATA untouched
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== drop-heartbeat: every SENSOR_HEARTBEAT lost for 1 s: safe state after the 350 ms timeout, recovery after it returns
  gateway dropped 10 SENSOR_HEARTBEAT frames
  SAFE_STATE command 374.7 ms after the last delivered heartbeat (timeout 350 ms + supervisor tick + command period)
  NORMAL command 514.6 ms after the first heartbeat delivered again (500 ms hold + tick + period)
  [ok] SAFE_STATE 340..420 ms after the last heartbeat
  [ok] NORMAL 500..600 ms after the first heartbeat again
  [ok] controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, RUNNING again
  [ok] gateway dropped 9..11 heartbeats in 1 s
  [ok] sensor unaffected (commands keep coming in safe-state mode): RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== drop-random: 10 % random loss in both directions for 3 s: the counter accounts for every lost frame
  SENSOR_DATA: 29 dropped by the gateway; controller counted 29 lost by counter and 0 timeouts (0 gaps over 35 ms hold 0 frames)
  SENSOR_HEARTBEAT: 4 dropped by the gateway; controller counted 4 lost by counter and 0 timeouts (0 gaps over 350 ms hold 0 frames)
  CONTROLLER_COMMAND: 17 dropped by the gateway; sensor counted 17 lost by counter and 0 timeouts (0 gaps over 70 ms hold 0 frames)
  gateway: a->b dropped 35 of 554, b->a dropped 19 of 277
  [ok] SENSOR_DATA timeouts == gaps over the timeout
  [ok] SENSOR_DATA dropped == counted lost + frames inside timeout gaps
  [ok] SENSOR_HEARTBEAT timeouts == gaps over the timeout
  [ok] SENSOR_HEARTBEAT dropped == counted lost + frames inside timeout gaps
  [ok] CONTROLLER_COMMAND timeouts == gaps over the timeout
  [ok] CONTROLLER_COMMAND dropped == counted lost + frames inside timeout gaps
  [ok] controller safe_state_entries == 0 (loss events on critical inputs)
  [ok] no CRC faults on either side (loss is not corruption)
  [ok] both RUNNING again
=== flood: 2000 frames/s of id 0x001 on the controller's segment for 2 s: counted, not disruptive
  flood: 4001 frames in 2.001 s (1999.9 /s achieved of 2000 requested), 0 send errors
  controller: 4001 unknown frames counted, 0 dropped by a full receive queue, queue high-water mark/depth 5/32
  vcan1 CONTROLLER_COMMAND      95 frames   50.0/s (expected 50)
  vcan1 SENSOR_DATA            190 frames  100.0/s (expected 100)
  [ok] controller counted at least 90 % of the flood as unknown frames
  [ok] flood reached at least 1000 frames/s
  [ok] vcan1 CONTROLLER_COMMAND rate within 10 %
  [ok] vcan1 SENSOR_DATA rate within 10 %
  [ok] controller unaffected by the flood: RUNNING, no CRC/counter faults, timeouts or safe-state entries
  [ok] flood stayed on its segment: the sensor saw no unknown frames
  [ok] sensor unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== hook-corrupt-crc: firmware CORRUPT_CRC: two bad frames are rejected and tolerated, five are a timeout and a safe state
  CORRUPT_CRC x2: sensor corrupted 2 frames, controller rejected 2 by CRC, 0 timeouts
  CORRUPT_CRC x5: sensor corrupted 5 frames, controller rejected 5 by CRC, SENSOR_DATA timeouts 1, safe_state_entries 1 (40+ ms without an accepted frame)
  [ok] x2: corrupted == rejected == 2
  [ok] x2: no timeout, no safe state
  [ok] x5: corrupted == rejected == 5
  [ok] x5: one SENSOR_DATA timeout and one safe-state entry
  [ok] controller RUNNING again
=== hook-skip-counter: firmware SKIP_COUNTER x3: each frame arrives with counter +2, accepted with one frame counted lost
  sensor skipped 3 counter values; controller: 3 accepted with loss, 3 frames counted lost, 0 rejected
  [ok] skipped == accepted-with-loss == lost == 3
  [ok] nothing rejected, no timeout, no safe state
=== hook-freeze-counter: firmware FREEZE_COUNTER x2: a stuck counter is rejected as repeated, twice is tolerated
  sensor repeated the previous counter on 2 frames; controller rejected 2 as repeated
  [ok] frozen == rejected == 2
  [ok] no CRC fault, no timeout, no safe state
=== hook-suppress-heartbeat: firmware SUPPRESS_HEARTBEAT 600 ms: the controller enters its safe state and recovers
  sensor suppressed 6 heartbeats; heartbeat silence on vcan0 700.6 ms
  SAFE_STATE 355.4 ms after the last heartbeat, NORMAL 514.9 ms after the first one again
  [ok] sensor suppressed 5..7 heartbeats
  [ok] SAFE_STATE 340..420 ms after the last heartbeat
  [ok] NORMAL 500..600 ms after the first heartbeat again
  [ok] controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, RUNNING again
=== hook-suppress-message: firmware SUPPRESS_MESSAGE CONTROLLER_COMMAND 300 ms: the sensor times out and degrades, then recovers
  controller suppressed 15 commands; CONTROLLER_COMMAND silence on vcan1: 320 ms
  sensor log: [00:00:36.340,000] <wrn> ecu: timeout: CONTROLLER_COMMAND not received for 70 ms; [00:00:36.340,000] <inf> ecu: node_state: RUNNING -> DEGRADED (critical input lost); [00:00:36.590,000] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal)
  [ok] controller suppressed 13..17 commands in 300 ms
  [ok] one command gap of 290..340 ms on the bus
  [ok] sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again
  [ok] sensor never in safe state (no actuator)
  [ok] controller unaffected: RUNNING, no CRC/counter faults, timeouts or safe-state entries
=== hook-stall-thread: firmware STALL_THREAD tx 1500 ms: the liveness monitor withholds the feed and the watchdog resets the node
  SENSOR_DATA silence on vcan0: 1313 ms
  sensor uptime after: 1710 ms; process kept its pid
  sensor log: [00:00:37.980,000] <wrn> ecu: test hook: tx thread blocked for 1500 ms
  sensor log: [00:00:38.275,000] <wrn> ecu: watchdog not fed: stale threads 0x2
  sensor log: *** Booting Zephyr OS build 75f67d766726 ***
  sensor log: [00:00:00.000,000] <inf> hal_can: CAN can ready (manual recovery no, test hooks on)
  sensor log: [00:00:00.000,000] <inf> hal_wdt: watchdog wdt armed, 1000 ms
  sensor log: [00:00:00.000,000] <inf> ecu: ecu role=sensor node=1 rx_prio=3 tx_prio=4 sup_prio=5 rx_queue=32 sup_tick=5 ms wdt=1000 ms
  [ok] sensor logged the stalled tx thread and the withheld watchdog feed
  [ok] watchdog reset the node in place (new boot banner, uptime restarted, same process)
  [ok] sensor silent 1000..1400 ms (300 ms liveness window + 1000 ms watchdog timeout, then reboot)
  [ok] controller: safe state entered once, RUNNING again
  [ok] sensor RUNNING again
--- sensor `ecu status`
  ecu: role=sensor test_hooks=1 uptime_ms=1750
  node: state=RUNNING reason=inputs established since_ms=1655 safe_state_entries=0 error_flags=0x00
  can: phase=active state=ERROR_ACTIVE tec=0 rec=0 manual_recovery=0 simulated=0 bus_off=0 passive=0 recovery_attempts=0 recoveries_ok=0 recovery_failures=0 last_recovery_ms=-1 backoff_ms=100
  tx: sent=210 ok=209 err=0 busy=0 rejected=0 blocked_bus_off=0 tick_overruns=0
  rx_summary: frames=104 unknown=0 queue_hwm=2/32 dropped=0 events_dropped=0 echo_responses=0 test_commands=0
  rx CONTROLLER_COMMAND: alive=1 age_ms=17 ok=87 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  rx CONTROLLER_HEARTBEAT: alive=1 age_ms=56 ok=17 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  wdt: available=1 timeout_ms=1000 feeds=350 skipped=0 stale_threads=0x0
  hooks: suppressed=0 corrupted=0 skipped=0 frozen=0
--- controller `ecu status`
  ecu: role=controller test_hooks=1 uptime_ms=41040
  node: state=RUNNING reason=inputs healthy again since_ms=1190 safe_state_entries=4 error_flags=0x04
  can: phase=active state=ERROR_ACTIVE tec=0 rec=0 manual_recovery=0 simulated=0 bus_off=0 passive=0 recovery_attempts=0 recoveries_ok=0 recovery_failures=0 last_recovery_ms=-1 backoff_ms=100
  tx: sent=2447 ok=2447 err=0 busy=0 rejected=0 blocked_bus_off=0 tick_overruns=0
  rx_summary: frames=8775 unknown=4001 queue_hwm=5/32 dropped=0 events_dropped=0 echo_responses=0 test_commands=17
  rx SENSOR_DATA: alive=1 age_ms=3 ok=193 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=2
  rx SENSOR_HEARTBEAT: alive=1 age_ms=91 ok=19 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=3
  rx BATTERY_STATUS: alive=1 age_ms=23 ok=20 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=2
  wdt: available=1 timeout_ms=1000 feeds=8208 skipped=0 stale_threads=0x0
  hooks: suppressed=0 corrupted=0 skipped=0 frozen=0
=== summary
  baseline                 ok    vcan0 BATTERY_STATUS          31 frames   10.0/s (expected 10)
  delay-jitter             ok    SENSOR_DATA latency vcan0->vcan1 with 15 +/- 5 ms configured: p1 10.2 ms, median 15.2 ms, p99 20.0 ms (n=285)
  duplicate                ok    gateway duplicated 34 of 362 sensor->controller frames; controller rejected 34 frames as repeated counter
  reorder                  ok    gateway swapped 39 SENSOR_DATA frames with their successor (0 released late without one)
  corrupt                  ok    gateway corrupted 20 of 423 frames; controller rejected 20 by CRC
  corrupt-battery          ok    4 consecutive BATTERY_STATUS frames corrupted; controller rejected 4 by CRC
  drop-heartbeat           ok    gateway dropped 10 SENSOR_HEARTBEAT frames
  drop-random              ok    SENSOR_DATA: 29 dropped by the gateway; controller counted 29 lost by counter and 0 timeouts (0 gaps over 35 ms hold 0 frames)
  flood                    ok    flood: 4001 frames in 2.001 s (1999.9 /s achieved of 2000 requested), 0 send errors
  hook-corrupt-crc         ok    CORRUPT_CRC x2: sensor corrupted 2 frames, controller rejected 2 by CRC, 0 timeouts
  hook-skip-counter        ok    sensor skipped 3 counter values; controller: 3 accepted with loss, 3 frames counted lost, 0 rejected
  hook-freeze-counter      ok    sensor repeated the previous counter on 2 frames; controller rejected 2 as repeated
  hook-suppress-heartbeat  ok    sensor suppressed 6 heartbeats; heartbeat silence on vcan0 700.6 ms
  hook-suppress-message    ok    controller suppressed 15 commands; CONTROLLER_COMMAND silence on vcan1: 320 ms
  hook-stall-thread        ok    SENSOR_DATA silence on vcan0: 1313 ms
RESULT: OK
```

The controller's cumulative counters at the end add up: four safe-state
entries (drop-heartbeat, CORRUPT_CRC x5, SUPPRESS_HEARTBEAT, STALL_THREAD),
three SENSOR_HEARTBEAT timeouts (the same minus CORRUPT_CRC), two SENSOR_DATA
timeouts (CORRUPT_CRC x5, STALL_THREAD), two BATTERY_STATUS timeouts
(corrupt-battery's fourth frame, STALL_THREAD), and 4001 unknown frames from
the flood.

The 1313 ms of silence in the stall scenario decomposes into the 300 ms
liveness window (the supervisor keeps feeding until the tx thread's last
check-in is that old), the 1000 ms watchdog timeout from the last feed, and
the re-execution of the simulator (`CONFIG_NATIVE_SIM_REBOOT`), which keeps
the pid, the `--can-if` argument and the shell's stdin: the test driver talked
to the rebooted node without reconnecting.

## 5. Repeatability: a second seed, and the Phase 2 demos again

```
$ python3 tools/fault_scenarios.py --retries 2 --seed 2
  relay latency SENSOR_DATA vcan0->vcan1: median 0.135 ms, p99 0.302 ms, max 0.422 ms (n=303)
  delay-jitter: p1 10.3 ms, median 15.2 ms, p99 20.1 ms (n=285)
  duplicate: 46 duplicated, 46 rejected        reorder: 29 swaps, 29 + 29
  corrupt: 22 corrupted, 22 rejected, longest run 2, no state change
  corrupt-battery: 3 rejected, DEGRADED for 100 ms
  drop-random: a->b 41 of 556 dropped, b->a 21 of 277; SENSOR_DATA: 36 dropped == 33 counted lost
               + 3 inside the one gap over 35 ms; 1 timeout and 1 safe-state entry, as derived
  flood: 4001 frames at 1999.9 /s, 0 dropped, queue high-water mark 5/32
  hook-suppress-message: 15 suppressed, 320 ms gap, DEGRADED for 250 ms
  hook-stall-thread: 1321 ms of silence, same process, rebooted
RESULT: OK
```

All checks passed with the second seed too, and this seed happened to produce
what the first did not: a run of three lost SENSOR_DATA frames, which is a
40 ms gap, a timeout and a safe-state entry. The expectations derived from the
capture (`timeouts == gaps over 35 ms`, `dropped == counted lost + frames
inside those gaps`, `safe_state_entries == loss events`) held exactly, which
is the point of deriving them. An earlier seed-2 run on the pre-review
firmware reported one relayed SENSOR_DATA gap of 31.3 ms on `vcan1` with no
counterpart on `vcan0` (the gateway process not scheduled for about 20 ms);
it stayed under the 35 ms timeout, no criterion failed, and the run was OK.
Had it exceeded the timeout, the gateway-stall classification would have made
the run INCONCLUSIVE and repeated it instead of reporting a firmware fault.

```
$ python3 tools/ecu_network.py --retries 2 run --duration 3        RESULT: OK
$ python3 tools/ecu_network.py --retries 2 safe-state
detection latency from last SENSOR_DATA      : 56.2 ms
recovery time from first SENSOR_HEARTBEAT    : 506.0 ms              RESULT: OK
$ python3 tools/ecu_network.py --retries 2 bus-off
CONTROLLER_COMMAND gap of 120.0 ms (controller silent while bus-off)
ERROR_PASSIVE visible in 19 heartbeats                               RESULT: OK
```

The Phase 2 demos still pass with the hooked transmit path (every role now
obtains its counters through `app::TxCounter` and `app::protect`, and
`transmit()` settles the applied faults).

## 6. Review of the firmware hooks

Reduced adversarial review, per the policy adopted for this phase: one finder
with a firmware/RTOS lens over `git diff HEAD -- firmware/app`, one verifier
per finding reading the code and the Zephyr v4.3.1 sources in the image (six
agents in all). Five candidates, five confirmed (two medium, three low), all
fixed before the runs recorded in sections 3 to 5:

1. (medium) `LOG_INF` ran under the spinlock in every new `TEST_CONTROL`
   case. In immediate log mode the shell log backend takes a semaphore and
   writes to the console inside the critical section, with interrupts masked;
   with asserts on, Zephyr's spin validation would report a context switch
   while holding a lock. Fixed: each case updates the state in its own
   `Locked` scope and logs after it, the pattern the rest of the firmware
   already follows (and that `ecu.hpp` states as the rule).
2. (medium) `STALL_THREAD` on the supervisor was invisible to the liveness
   monitor: the supervisor checked itself in just before evaluating the stale
   mask, so its own entry could never be stale, and a supervisor stall shorter
   than the watchdog period left no flag, no counter and no log line. Fixed:
   the supervisor checks in after its tick's work, so the next tick judges the
   gap the way it judges the RX and TX threads.
3. (low) The DBC text of `STALL_THREAD` promised a reset for any stall longer
   than the liveness window. The feed decision is level-triggered, so only a
   stall that outlasts the window plus the watchdog period resets the node
   (about 1.3 s; 1.0 to 2.2 s on the Nucleo, whose IWDG runs from the 17 to
   47 kHz LSI), and a shorter one leaves the warning flag and a skipped-feed
   count. Fixed in the DBC comment (header regenerated), the `ecu.cpp`
   comment, `docs/fault-model.md` and `DESIGN.md`.
4. (low) The `CORRUPT_CRC`, `SKIP_COUNTER` and `FREEZE_COUNTER` budgets were
   consumed and counted in `protect()`, before `transmit()` decided whether
   the frame left the node, so with a bus-off or a suppressed message the hook
   counters could report faults that never reached the bus. Fixed: the faults
   applied to a frame travel with it to `transmit()`, which counts them when
   the frame was sent and hands them back to the budget otherwise.
5. (low) `ecu reset` did not say that pending test faults and the hook
   counters are cleared too. Fixed in the output and the help text.

Nothing in the review changed a scenario's expected numbers: the fixes concern
locking discipline, the supervisor's own liveness and accounting in fault
combinations the scenarios do not exercise, and all fifteen scenarios pass
unchanged on the fixed firmware with both seeds (sections 4 and 5).

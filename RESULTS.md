# Results: raw data behind the numbers

Every number quoted in the README comes from a run recorded here or in a phase
log, with the command that produced it. All of it was **measured on a simulated
bus (Linux `vcan`) on a desktop OS, not on real-time hardware**: the ECUs are
Zephyr `native_sim` processes, timestamps are the Linux kernel's receive
timestamps of the `vcan` interface, and the host was Docker Desktop 4.88.1 on
WSL2 (kernel 6.6.87.2) on Windows 11, run through `tools/dev.sh`. Two
consequences shape how to read the timing numbers:

- **Jitter and latency measure the host, not the firmware.** The simulator
  executes firmware code in zero simulated time and maps simulated time onto
  host time with the host's timer resolution and scheduling latency. The
  firmware's own clocks (timeouts, recovery hold, liveness window) are exact;
  the spacing of frames on the bus and the echo round trip vary from run to run
  with the host's load, which is why two runs are kept below.
- **Bus load is a property of the traffic, not of `vcan`.** `vcan` has no bit
  rate. The load is what the recorded frames would occupy on a CAN 2.0A bus at
  500 kbit/s, computed from each frame's DLC: 47 + 8·DLC bits without stuff
  bits, plus ⌊(34 + 8·DLC − 1)/4⌋ with worst-case bit stuffing.

Sections are added phase by phase; Phase 8 consolidates them into the README's
results table.

## 1. Phase 4: recording of the plain-bus network with the host tools

Command (inside the dev image; the CI step runs the same):

```bash
tools/dev.sh python3 tools/record_and_analyze.py --seed 2
```

It starts both ECUs on `vcan0`, records the bus with `python3 -m host.logger`
(`bus.log`, `bus.csv`), watches it with `python3 -m host.decoder --mode table`,
sends 200 `ECHO_REQUEST` probes to both ECUs 20 ms apart with `host.echo`, and
runs `python3 -m host.analysis` on the recording. The logs of each run are
published to `build/network-logs/record-and-analyze/` (`bus.log`, `bus.csv`,
`analysis.json`, `decoder.log`, `logger.log`, both ECU consoles, `summary.log`).
The recording was also checked by the independent verifier:

```
$ python3 can/verify_candump.py --min-protected 1000 build/network-logs/record-and-analyze/bus.log
verified 2038 frames (1438 protected, CRC-checked): 0 wrong DLC, 0 bad CRC, 0 counter faults, 0 unknown ids, 0 unparsed lines -> OK
```

### 1.1 Run 2 (seed 2), the run quoted in the README

```
capture: bus.log: 2040 frames over 7.991 s on vcan0; unknown identifiers: none
protection: 1440 protected frames, dlc faults 0, crc faults 0, repeated counters 0, out-of-sequence 0, frames lost per the counters 0
every timing number below: measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware

bus load at 500 kbit/s (CAN 2.0A, 11-bit identifiers: what this traffic would occupy on a real bus; vcan has no bit rate)
  message                     id  frames  rate/s dlc    bit/s   share  worst-case bit/s
  CONTROLLER_COMMAND       0x080     400    50.1   8     5557   20.2 %              6758
  SENSOR_DATA              0x100     800   100.1   8    11113   40.4 %             13516
  BATTERY_STATUS           0x110      80    10.0   8     1111    4.0 %              1352
  SENSOR_HEARTBEAT         0x200      80    10.0   8     1111    4.0 %              1352
  CONTROLLER_HEARTBEAT     0x201      80    10.0   8     1111    4.0 %              1352
  ECHO_REQUEST             0x300     200    25.0   6     2378    8.6 %              2878
  SENSOR_ECHO_RESPONSE     0x301     200    25.0   7     2578    9.4 %              3129
  CONTROLLER_ECHO_RESPONSE 0x302     200    25.0   7     2578    9.4 %              3129
  total 27538 bit/s = 5.51 % of 500 kbit/s (worst-case stuffing 6.69 %)
  periodic messages 4.00 % measured vs 4.00 % nominal from the DBC cycle times (4.86 % worst case); busiest 100 ms window 7.42 %

period and jitter in ms (consecutive frames of one identifier; gaps: periods over 3 x nominal, a process not scheduled)
  message                  nominal      n  rate/s     mean    std     min      max     p50     p99 peak dev  gaps
  CONTROLLER_COMMAND          20.0    400    50.0   20.001  0.078   19.69    20.32   20.00   20.26     0.32     0
  SENSOR_DATA                 10.0    800   100.0   10.001  0.101    9.50    10.53   10.00   10.25     0.53     0
  BATTERY_STATUS             100.0     80    10.0  100.005  0.274   98.97   100.33  100.09  100.22     1.03     0
  SENSOR_HEARTBEAT           100.0     80    10.0  100.008  0.264   99.15   100.36  100.08  100.34     0.85     0
  CONTROLLER_HEARTBEAT       100.0     80    10.0  100.007  0.286   99.00   100.34  100.09  100.31     1.00     0
  event-driven: CONTROLLER_ECHO_RESPONSE x200, ECHO_REQUEST x200, SENSOR_ECHO_RESPONSE x200

responder         sent  answ  unm   rtt min     p50     p90     p99     max   turnaround p50     p99   rtt-turnaround p50     p99
CONTROLLER_ECU     200   200    0     0.044   0.582   1.029   1.195   1.204            0.000   0.000                0.582   1.195
SENSOR_ECU         200   200    0     0.031   0.544   1.018   1.155   1.187            0.000   0.000                0.544   1.155
all in ms. rtt: ECHO_REQUEST on the bus to the response on the bus (kernel timestamps); turnaround: the ECU's own receive-to-transmit time from its response; the difference is the kernel, the CAN driver and the OS scheduling of the ECU process
turnaround is 0 in every response: the ECU's code ran in zero simulated time, as it does on Zephyr's native_sim, so the whole round trip is host time (the field is informative on hardware)
```

The probe's own live matching gave the same numbers (same kernel timestamps,
different socket), and both ECUs reported `echo_responses=200` in `ecu status`,
equal to the 200 responses each matched in the recording. Both ECUs ended
RUNNING with `safe_state_entries=0`, `dropped=0`, no skipped watchdog feed.

### 1.2 Run 1 (seed 1), the same command on a busier host

Same code, run a few minutes earlier while other containers had just exited.
Only the timing columns differ:

```
period and jitter in ms
  message                  nominal      n  rate/s     mean    std     min      max     p50     p99 peak dev  gaps
  CONTROLLER_COMMAND          20.0    400    50.0   19.996  0.472   18.28    21.35   19.99   21.26     1.72     0
  SENSOR_DATA                 10.0    798   100.0    9.997  0.428    8.38    11.43   10.00   11.17     1.62     0
  BATTERY_STATUS             100.0     80    10.0   99.983  0.331   98.11   100.44  100.08  100.38     1.89     0
  SENSOR_HEARTBEAT           100.0     80    10.0   99.989  0.590   97.91   101.30  100.08  101.30     2.09     0
  CONTROLLER_HEARTBEAT       100.0     80    10.0   99.983  0.557   98.00   101.29  100.07  101.26     2.00     0

responder         sent  answ  unm   rtt min     p50     p90     p99     max
CONTROLLER_ECU     200   200    0     0.086   0.723   1.643   2.166   2.248
SENSOR_ECU         200   200    0     0.117   0.710   1.726   2.220   4.821

bus load: total 27551 bit/s = 5.51 % (worst case 6.70 %); periodic 4.00 % vs 4.00 % nominal; busiest 100 ms window 7.67 %
```

The means and the bus load are identical to run 2; the spread (standard
deviation four times larger, one 4.8 ms round trip) is the host. Neither run
had a gap longer than three periods, so neither was classified as a host stall.

### 1.3 Where the round trip comes from

The `native_sim` SocketCAN driver (`drivers/can/can_native_linux.c` in Zephyr
v4.3.1) polls its socket with a zero-timeout `select()` and then sleeps one
simulated millisecond ("short sleep required to avoid blocking the whole native
process"), so a request waits 0–1 ms for the next poll. The firmware's own
processing takes no simulated time, which is why every response carries
`turnaround = 0 µs`: `k_cycle_get_32()` on `native_sim` reads the simulated
clock. The transmit is a `write()` on the socket. A median of 0.5–0.7 ms and a
99th percentile of 1.2–2.2 ms are therefore the poll interval plus the host's
scheduling, not a property of the firmware; the Phase 3 gateway, a Python
process on the same host, relays a frame in 0.13 ms median. On the Nucleo the
same probe would measure the two frames' bus time (95 and 103 bits, about
0.2 ms each at 500 kbit/s) plus a real interrupt-to-thread-to-transmit path,
and the turnaround field would be filled from the cycle counter.

### 1.4 The same command on a GitHub-hosted runner

CI runs the same command (`tools/dev.sh python3 tools/record_and_analyze.py
--retries 2`, the last `vcan` step of `.github/workflows/ci.yml`) on a
GitHub-hosted runner. The run for the Phase 4 commit
([40799295827](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40799295827)) passed on
its first attempt, and its `network-logs` artifact holds `bus.log`, `bus.csv`,
`analysis.json` and the console output. The analysis of that recording, the
quietest of the three runs (SENSOR_DATA standard deviation 0.062 ms against
0.10 and 0.43 ms in sections 1.1 and 1.2):

```
capture: bus.log: 2042 frames over 8.000 s on vcan0; unknown identifiers: none
protection: 1442 protected frames, dlc faults 0, crc faults 0, repeated counters 0, out-of-sequence 0, frames lost per the counters 0
every timing number below: measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware

bus load at 500 kbit/s (CAN 2.0A, 11-bit identifiers: what this traffic would occupy on a real bus; vcan has no bit rate)
  message                     id  frames  rate/s dlc    bit/s   share  worst-case bit/s
  CONTROLLER_COMMAND       0x080     401    50.1   8     5564   20.2 %              6767
  SENSOR_DATA              0x100     801   100.1   8    11114   40.4 %             13516
  BATTERY_STATUS           0x110      80    10.0   8     1110    4.0 %              1350
  SENSOR_HEARTBEAT         0x200      80    10.0   8     1110    4.0 %              1350
  CONTROLLER_HEARTBEAT     0x201      80    10.0   8     1110    4.0 %              1350
  ECHO_REQUEST             0x300     200    25.0   6     2375    8.6 %              2875
  SENSOR_ECHO_RESPONSE     0x301     200    25.0   7     2575    9.4 %              3125
  CONTROLLER_ECHO_RESPONSE 0x302     200    25.0   7     2575    9.4 %              3125
  total 27532 bit/s = 5.51 % of 500 kbit/s (worst-case stuffing 6.69 %)
  periodic messages 4.00 % measured vs 4.00 % nominal from the DBC cycle times (4.86 % worst case); busiest 100 ms window 7.61 %

period and jitter in ms (consecutive frames of one identifier; gaps: periods over 3 x nominal, a process not scheduled)
  message                  nominal      n  rate/s     mean    std     min      max     p50     p99 peak dev  gaps
  CONTROLLER_COMMAND          20.0    401    50.0   19.999  0.070   19.75    20.24   20.00   20.17     0.25     0
  SENSOR_DATA                 10.0    801   100.0   10.000  0.062    9.44    10.56   10.01   10.13     0.56     0
  BATTERY_STATUS             100.0     80    10.0  100.003  0.301   98.91   100.22  100.09  100.21     1.09     0
  SENSOR_HEARTBEAT           100.0     80    10.0  100.003  0.270   99.10   100.21  100.09  100.21     0.90     0
  CONTROLLER_HEARTBEAT       100.0     80    10.0  100.001  0.286   98.90   100.35  100.09  100.31     1.10     0
  event-driven: CONTROLLER_ECHO_RESPONSE x200, ECHO_REQUEST x200, SENSOR_ECHO_RESPONSE x200

responder         sent  answ  unm   rtt min     p50     p90     p99     max   turnaround p50     p99   rtt-turnaround p50     p99
CONTROLLER_ECU     200   200    0     0.047   0.594   1.064   1.157   1.170            0.000   0.000                0.594   1.157
SENSOR_ECU         200   200    0     0.066   0.579   1.029   1.114   1.122            0.000   0.000                0.579   1.114
all in ms. rtt: ECHO_REQUEST on the bus to the response on the bus (kernel timestamps); turnaround: the ECU's own receive-to-transmit time from its response; the difference is the kernel, the CAN driver and the OS scheduling of the ECU process
turnaround is 0 in every response: the ECU's code ran in zero simulated time, as it does on Zephyr's native_sim, so the whole round trip is host time (the field is informative on hardware)
```

Both ECUs finished RUNNING with `safe_state_entries=0`, `echo_responses=200`,
`dropped=0` and `wdt skipped=0`; `RESULT: OK`.

## 2. Phase 5: the system test suite, every measured value

Command: `tools/dev.sh tools/run-system-tests.sh --coverage` on 2026-10-29 (the
run quoted in the README and in DESIGN.md section 14): 23 of 23 tests
passed in 94 s, seed 1, no attempt inconclusive. The suite (`tests/`) drives
both ECUs through the Phase 3 gateway (sensor on `vcan0`, controller on
`vcan1`) and judges each test from the ECUs' `ecu status` counters, the
gateway's counters and the frames captured on both segments; the firmware's
constants come from the controller build's `.config`. The ECUs were the
application-instrumented builds (`tools/build-ecu.sh native_sim <role>
--coverage`), which is also where the firmware coverage of section 2.2 comes
from. The JUnit report, `measurements.json`, this table (`measurements.md`) and
the ECU, gateway and `candump` logs of every module are what
`build/test-results/` holds after a run, and what CI uploads as the
`system-tests` artifact.

How to read the limits: a reaction published by a periodic message can appear
no earlier than the timeout (minus 4 ms, the firmware's clock against the
kernel's) and no later than timeout + supervisor tick (5 ms) + one period of
the publishing message (20 ms for `CONTROLLER_COMMAND.mode`, 100 ms for a
heartbeat's `node_state` or flags) + 40 ms for the host's scheduling of
desktop processes; `expected` rows are predicates on counters, with the counts
behind them in the value column; `info` rows are measured without a limit.

### 2.1 Measured values, `measurements.md` of the run

| test | measurement | value | limit | result |
|---|---|---|---|---|
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood rate achieved by the gateway (2000/s requested) (4001 frames in 2.001 s, 0 send errors; Python sender on a desktop OS) | 1999.9 frames/s | >= 1800.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood frames counted as unknown by the controller (4001 of 4001) | 100.0 % | >= 90.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | no frame dropped by a full receive queue | 0 dropped, queue high-water mark 5/32 | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | receive queue high-water mark / depth after the flood | 5/32 frames |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 during the flood (n=94, max 20.7 ms) | 20.3 ms | 18.0..22.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period max during the flood | 20.7 ms | <= 25.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 after the flood (n=21, max 20.3 ms) | 20.2 ms |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | SENSOR_DATA accepted by the controller during the experiment | 99.9 frames/s | 95.0..105.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | controller: no timeouts, no CRC or counter faults, no safe state, RUNNING |  | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | the flood stayed on its segment: the sensor saw no unknown frames, is RUNNING |  | expected | ok |
| test_flood_rate_sweep | 1000/s requested: achieved (1001 frames) | 999.9 frames/s |  | info |
| test_flood_rate_sweep | 1000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 1000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 1000/s: CONTROLLER_COMMAND period max during the flood (p99 20.35 ms) | 20.4 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 1000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 2000/s requested: achieved (2001 frames) | 1999.6 frames/s |  | info |
| test_flood_rate_sweep | 2000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 2000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 2000/s: CONTROLLER_COMMAND period max during the flood (p99 20.18 ms) | 20.2 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 2000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 4000/s requested: achieved (4002 frames) | 3999.4 frames/s |  | info |
| test_flood_rate_sweep | 4000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 4000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 7/32 | expected | ok |
| test_flood_rate_sweep | 4000/s: CONTROLLER_COMMAND period max during the flood (p99 20.17 ms) | 20.2 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 4000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_error_passive_entry_and_healing | ERROR_PASSIVE visible in the controller heartbeat after the injection (INJECT_CAN_ERROR x20: TEC 160) | 31.0 ms | 0..145.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (firmware log, ECU clock) (TEC heals by one per successful frame at 60/s: 33 frames, 550 ms expected) | 530.0 ms | 490.0..610.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (heartbeats, 100 ms granularity) | 599.7 ms |  | info |
| test_error_passive_entry_and_healing | CONTROLLER_COMMAND rate while error-passive (transmission continues) | 50.0 frames/s | 45.0..55.0 | ok |
| test_error_passive_entry_and_healing | one passive event, no bus-off, no safe state (error-passive is not an input loss) |  | expected | ok |
| test_error_passive_entry_and_healing | controller ERROR_ACTIVE and RUNNING again, TEC healed | state ERROR_ACTIVE tec=69 | expected | ok |
| test_bus_off_recovery_after_the_backoff | exactly one gap in the controller's commands | 1 | expected | ok |
| test_bus_off_recovery_after_the_backoff | CONTROLLER_COMMAND silence on the bus (bus-off, backoff, recovery) (backoff 100 ms + modelled recovery 3 ms + supervisor tick + next TX slot) | 140.0 ms | 100..168.0 | ok |
| test_bus_off_recovery_after_the_backoff | bus-off to error-active in the firmware log (ECU clock) | 105.0 ms | 100..118 | ok |
| test_bus_off_recovery_after_the_backoff | last_recovery_ms reported by `ecu status` | 104 ms |  | info |
| test_bus_off_recovery_after_the_backoff | one bus-off, one attempt, one recovery ok, no failure, phase active | phase active | expected | ok |
| test_bus_off_recovery_after_the_backoff | bus-off is an input loss: one safe-state entry, the first command after the gap is SAFE_STATE | first mode SAFE_STATE | expected | ok |
| test_bus_off_recovery_after_the_backoff | NORMAL again after the first command following recovery (recovery hold 500 ms) | 499.9 ms | 496..565.0 | ok |
| test_bus_off_recovery_after_the_backoff | heartbeat after recovery: BUS_OFF_SEEN flag set, bus_off_count 1 |  | expected | ok |
| test_bus_off_recovery_after_the_backoff | sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again | [00:00:02.650,000] <inf> ecu: node_state: RUNNING -> DEGRADED (critical input lost); [00:00:02.720,000] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal) | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #1: command silence (backoff 100 ms) | 140.0 ms | 100..168.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #2: command silence (backoff 200 ms (doubled: the previous recovery did not hold)) | 220.0 ms | 200..268.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #3: command silence (backoff 400 ms (doubled: the previous recovery did not hold)) | 420.0 ms | 400..468.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #4: command silence (backoff 800 ms (doubled: the previous recovery did not hold)) | 820.0 ms | 800..868.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #5: command silence (backoff 1600 ms (doubled: the previous recovery did not hold)) | 1620.2 ms | 1600..1668.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | every attempted recovery completed before the next bus-off (5) | [True, True, True, True, True] | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #6: no recovery, phase failed, node FAULT, transmission stopped | phase failed, state FAULT, 0 commands after it | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | 5 attempts, 5 recoveries ok, 5 failures (the attempts are exhausted) | attempts 5, ok 5, failures 5 | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | first command after `ecu reset` (the reset restarts the backoff from the first attempt; the controller is still bus-off) | 127.4 ms | 100..188.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | NORMAL again after `ecu reset` (backoff 100 ms + recovery hold 500 ms) | 627.4 ms | 600..693.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | controller RUNNING, CAN phase active after the reset | state RUNNING, phase active | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | sensor: one CONTROLLER_COMMAND timeout per silence (6), RUNNING again | 6 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller CRC rejections == frames the gateway corrupted | 22 rejected, 22 corrupted of 422 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: runs of consecutive corrupted frames (longest 2; a run of rejects is silence to the 35 ms timeout) | 19 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 1 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 1 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller safe-state entries == timeout runs on critical inputs (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller RUNNING again; sensor unaffected |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | sensor corrupted 2 frames, controller rejected 2 by CRC | 2 corrupted, 2 bad on the bus, 2 rejected | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN clear in the heartbeat before the fault (RESET_FAULTS) |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN published in the controller heartbeat after the first bad frame (flag set on rejection, published by the next heartbeat) | 61.9 ms | 0..145.0 | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | two bad frames are tolerated: no timeout, no safe state |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | sensor repeated the previous counter on 2 frames, controller rejected 2 as repeated | 2 frozen, 2 stale on the bus, 2 rejected | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN clear before the fault |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN published after the first stale frame | 72.1 ms | 0..145.0 | ok |
| test_stale_counter_is_rejected_as_repeated | two stale frames are tolerated: no CRC fault, no timeout, no safe state | crc 0, timeouts 0, safe-state entries 0 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | controller repeated-counter rejections == frames the gateway duplicated | 41 rejected, 41 duplicated of 362 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | duplicates cause no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | sensor skipped 3 counter values: controller accepted 3 frames with loss, 3 frames counted lost | 3 skipped, 3 accepted with loss, 3 lost | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | nothing rejected (the data is fresh), no timeout, no safe state |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | SENSOR_DATA frames dropped for 15 ms (largest gap on the bus 30.0 ms) | 2 frames |  | info |
| test_skipped_counter_is_accepted_and_the_loss_counted | timeouts == gaps over 35 ms (0) | 0 | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | dropped == counted lost by the counter + frames inside timeout gaps | 2 dropped, 2 lost, 0 in timeout gaps | expected | ok |
| test_persistent_bad_frames_degrade_the_node | CRC rejections == corrupted BATTERY_STATUS frames (at least 3) | 4 rejected, 4 corrupted | expected | ok |
| test_persistent_bad_frames_degrade_the_node | DEGRADED in the heartbeat after the 3rd consecutive bad frame | 20.9 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | RUNNING again after the first good frame | 20.9 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | no safe state (battery is not a critical input), SENSOR_DATA untouched |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped by the gateway (controller counted 36 lost by counter and 0 timeouts; 0 gaps over 35 ms hold 0 frames) | 36 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped == counted lost + frames inside timeout gaps | 36 vs 36 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped by the gateway (controller counted 4 lost by counter and 0 timeouts; 0 gaps over 350 ms hold 0 frames) | 4 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped == counted lost + frames inside timeout gaps | 4 vs 4 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped by the gateway (sensor counted 13 lost by counter and 1 timeouts; 1 gaps over 70 ms hold 3 frames) | 16 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: timeouts == gaps over the timeout | 1 vs 1 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped == counted lost + frames inside timeout gaps | 16 vs 13 + 3 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | controller safe-state entries == loss events on its critical inputs (0) | 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | no CRC faults on either side (loss is not corruption); both RUNNING again |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | gateway: dropped sensor->controller / controller->sensor | 41 of 554 / 21 of 277 |  | info |
| test_duplicated_frames_are_rejected | controller repeated-counter rejections == frames the gateway duplicated | 34 rejected, 34 duplicated of 362 | expected | ok |
| test_duplicated_frames_are_rejected | COUNTER_FAULT_SEEN clear before, published in the controller heartbeat after |  | expected | ok |
| test_duplicated_frames_are_rejected | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p1 with 15 +/- 5 ms configured (n=285) | 10.4 ms | >= 9.500 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency median | 15.6 ms | 13.0..17.0 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p99 | 20.2 ms | <= 23.0 | ok |
| test_delayed_frames_are_not_losses | gateway max scheduled latency a->b / b->a | 19.98 / 19.98 ms |  | info |
| test_delayed_frames_are_not_losses | latency is not loss for a period-based timeout: no timeouts, no safe state, both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: SENSOR_DATA latency median | 20.2 ms | 19.0..22.0 | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: largest SENSOR_DATA gap on the controller's segment (the step into the delay) (period 10 ms + 20 ms step, below the 35 ms timeout) | 30.2 ms | <= 34 | ok |
| test_delayed_frames_are_not_losses | 20 ms of latency: no timeout, no counter fault, no safe state |  | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | SENSOR_DATA frames swapped with their successor (0 held frames released late without a successor) | 31 frames |  | info |
| test_reordered_frames_are_rejected_out_of_sequence | out-of-sequence rejections == swaps (the late frame is rejected) | 31 | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | accepted-with-loss events == swaps (the successor arrives with counter +2), one frame counted lost each | 31 accepted with loss, 31 lost | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_sensor_process_death_and_restart | SAFE_STATE command after the last SENSOR_DATA of the killed sensor (the 35 ms SENSOR_DATA timeout fires first; the heartbeat timeout would take 350 ms) | 50.0 ms | 31..100.0 | ok |
| test_sensor_process_death_and_restart | time from the kill to the SAFE_STATE command | 40.9 ms |  | info |
| test_sensor_process_death_and_restart | controller in SAFE_STATE while the sensor is dead | SAFE_STATE | expected | ok |
| test_sensor_process_death_and_restart | restarted sensor: first frame after the process start (process start, Zephyr boot, first TX tick) | 19.2 ms | <= 1000.0 | ok |
| test_sensor_process_death_and_restart | restarted sensor: first heartbeat after its first frame | 60.2 ms | 0..110.0 | ok |
| test_sensor_process_death_and_restart | NORMAL command after the first heartbeat of the restarted sensor (both critical inputs alive from the heartbeat on; hold 500 ms) | 507.5 ms | 496..565.0 | ok |
| test_sensor_process_death_and_restart | controller: one timeout per sensor message, one safe-state entry | {'SENSOR_DATA': 1, 'BATTERY_STATUS': 1, 'SENSOR_HEARTBEAT': 1} | expected | ok |
| test_sensor_process_death_and_restart | the restarted sensor's counters (from 0) are accepted: no counter rejections, no CRC faults (the timeout reset the receiver's counter history) |  | expected | ok |
| test_sensor_process_death_and_restart | both RUNNING at the end |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | exactly one gap in SENSOR_DATA | 1 | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | SENSOR_DATA silence on the bus (stall, watchdog reset, reboot, first TX tick) | 1313.2 ms |  | info |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog reset after the last frame (firmware uptime dates the reboot) (the supervisor feeds the watchdog until the stalled thread is 300 ms stale, then the watchdog's 1000 ms run out) | 1301.6 ms | 1290..1330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | reboot to the first frame of the rebooted sensor (host: exec, Zephyr boot, first TX tick) (host time, not firmware time: the executable is re-executed from the bind mount) | 11.6 ms | <= 1500.0 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog feed withheld after the liveness window (firmware log, ECU clock) (liveness window 300 ms) | 295.0 ms | 290..330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor rebooted in place: boot banner again, uptime restarted, same process | uptime 1700 ms, same pid | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | controller: one safe-state entry, RUNNING again |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor RUNNING again |  | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | heartbeats suppressed by the sensor (SUPPRESS_HEARTBEAT 600 ms) | 6 frames |  | info |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SENSOR_HEARTBEAT silence on the bus (600 ms of suppression starts and ends between two 100 ms heartbeat slots) | 700.9 ms | 590..810.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SAFE_STATE command after the last heartbeat (heartbeat timeout 350 ms + supervisor tick + command period) | 374.8 ms | 346..415.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | NORMAL command after the first heartbeat again (recovery hold 500 ms + tick + command period) | 513.9 ms | 496..565.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | every command in the safe state: actuators off, brake hold, speed limit 0, torque 0 | 42 commands over 840 ms | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, SENSOR_DATA never timed out, RUNNING again |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: frames dropped by the gateway (435 ms of loss, period 10 ms) | 44 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 35 ms + supervisor tick 5 ms + 20 ms publish period) | 51.3 ms | 31..100.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 521.4 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_DATA, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: frames dropped by the gateway (750 ms of loss, period 100 ms) | 7 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 20 ms publish period) | 371.0 ms | 346..415.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 511.2 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_HEARTBEAT, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: frames dropped by the gateway (750 ms of loss, period 100 ms) | 8 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller reaction (CONTROLLER_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 100 ms publish period) | 421.5 ms | 346..495.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 20.9 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: exactly one timeout counted by the controller, none for its other inputs | 1 for BATTERY_STATUS, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: frames dropped by the gateway (470 ms of loss, period 20 ms) | 24 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor reaction (SENSOR_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 70 ms + supervisor tick 5 ms + 100 ms publish period) | 168.7 ms | 66..215.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 69.0 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: exactly one timeout counted by the sensor, none for its other inputs | 1 for CONTROLLER_COMMAND, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: controller unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period mean (n=499, nominal 10 ms) | 10.0 ms | 9.900..10.1 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period std | 0.109 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period p99 | 10.3 ms | 9.000..11.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period min | 9.420 ms | >= 7.500 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period max | 10.5 ms | <= 12.5 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA rate | 100.0 frames/s | 98.0..102.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period std | 0.252 ms |  | info |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period p99 | 100.3 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period min | 99.1 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period max | 100.3 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS rate | 9.999 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period std | 0.297 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period p99 | 100.3 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period min | 99.1 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period max | 100.3 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period mean (n=249, nominal 20 ms) | 20.0 ms | 19.8..20.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period std | 0.104 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period p99 | 20.3 ms | 18.0..22.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period min | 19.7 ms | >= 15.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period max | 20.5 ms | <= 25.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND rate | 50.0 frames/s | 49.0..51.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period std | 0.329 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period p99 | 100.5 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period min | 99.0 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period max | 100.6 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | sensor: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | sensor: watchdog fed on every supervisor tick | feeds 1104, skipped 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: watchdog fed on every supervisor tick | feeds 1108, skipped 0 | expected | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.8 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.841 ms | <= 15.0 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.9 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.764 ms | <= 15.0 | ok |

### 2.2 Firmware line coverage under the suite

`gcovr` over the two application object directories of the instrumented
builds (`build/test-results/firmware-coverage.txt`): 90.5 % of
lines (939 of 1037), 92.7 % of
functions (89 of 96), 57.6 % of
branches (269 of 467). The uncovered lines
are initialisation failure paths, the allocation guard's trap, the Nucleo-only
HAL branches, the echo responder and the hook commands the suite does not use
(DESIGN.md section 14.7).

```
------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                         Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
firmware/app/src/alloc_guard.c                   9        3    33%   45,47,50,52-54
firmware/app/src/app/controller_role.cpp
                                               159      159   100%
firmware/app/src/app/ecu.cpp                   402      359    89%   81-82,93,98,100-102,104,107-115,140-143,145-147,175-177,225,229,258,315-316,322-325,361,506-507,511-512,517-518
firmware/app/src/app/ecu.hpp                    64       60    93%   161,161,163,163
firmware/app/src/app/sensor_role.cpp           117      116    99%   40
firmware/app/src/app/test_control.cpp           63       46    73%   24-25,83-89,91-93,96-100
firmware/app/src/hal/can_bus.cpp               161      144    89%   31-34,69-71,94-95,97,116,123,201,217,219-220,223
firmware/app/src/hal/can_bus.hpp                 1        1   100%
firmware/app/src/hal/watchdog.cpp               26       19    73%   24-25,35-36,40-42
firmware/app/src/hal/watchdog.hpp                2        2   100%
firmware/app/src/main.cpp                        8        5    62%   18-20
firmware/app/src/shell_cmds.c                   25       25   100%
------------------------------------------------------------------------------
TOTAL                                         1037      939    90%
------------------------------------------------------------------------------
```

### 2.3 Unit tests of the firmware libraries

`tools/run-host-tests.sh` (GoogleTest, gcovr with `-fkeep-inline-functions`),
77 cases: 100.0 % of lines (937 of 937),
100.0 % of functions, 85.4 % of branches of the
library headers.

```
------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                         Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
firmware/lib/ecu_core/include/ecu/can_error_sm.hpp
                                               107      107   100%
firmware/lib/ecu_core/include/ecu/error_counter_model.hpp
                                                38       38   100%
firmware/lib/ecu_core/include/ecu/liveness.hpp
                                                16       16   100%
firmware/lib/ecu_core/include/ecu/node_state.hpp
                                                62       62   100%
firmware/lib/ecu_core/include/ecu/protected_rx.hpp
                                                50       50   100%
firmware/lib/ecu_core/include/ecu/timeout_monitor.hpp
                                                74       74   100%
firmware/lib/protocol/include/protocol/codec_util.hpp
                                                74       74   100%
firmware/lib/protocol/include/protocol/crc8.hpp
                                                31       31   100%
firmware/lib/protocol/include/protocol/e2e.hpp
                                                64       64   100%
firmware/lib/protocol/include/protocol/messages.hpp
                                               421      421   100%
------------------------------------------------------------------------------
TOTAL                                          937      937   100%
------------------------------------------------------------------------------
```

## 3. Earlier phases

The numbers from Phases 2 and 3 quoted in the README are recorded with their
full console output in the phase logs:

| Number | Command | Raw data |
|---|---|---|
| Safe state 54.1 ms after the last sensor frame; back to normal 525 ms after the restarted sensor's first heartbeat | `tools/dev.sh python3 tools/ecu_network.py safe-state` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Simulated bus-off recovered in 103 ms, one 120 ms command gap | `tools/dev.sh python3 tools/ecu_network.py bus-off` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Relay latency median 0.13 ms, p99 0.27 ms; safe state 374.7 ms after a lost heartbeat; 50 of 50 dropped frames accounted for; 1313 ms of silence before the watchdog reset | `tools/dev.sh python3 tools/fault_scenarios.py --seed 1` | [docs/phase-logs/phase3.md](docs/phase-logs/phase3.md), [DESIGN.md §12.5](DESIGN.md#125-measured-results-simulated-bus-on-a-desktop-os) |

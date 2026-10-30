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

Section 0 maps every row of the README's results table to the run it comes
from, the commit that run was made on and the CI run that reproduced it;
sections 1 to 4 are the runs themselves, one per phase that measured
something, with the runner's reproduction after each local run.

## 0. Where each README number comes from

The README quotes one local run per kind of measurement; each was then
reproduced on a GitHub-hosted runner by the CI job, from the same commands,
in the same container image. The runs:

| Run | Command | Date and code | Recorded in | Reproduced by |
|---|---|---|---|---|
| A | `tools/dev.sh python3 tools/record_and_analyze.py --seed 2` (and `--seed 1`, the busier run) | 2026-10-28, Phase 4 code, commit `a3b7467` | section 1.1 (1.2 the busier run) | section 1.4, the runner's own recording in CI run 40799295827; the "Host tools on vcan" step of every CI run since |
| B | `tools/dev.sh python3 tools/fault_scenarios.py --seed 1` | 2026-10-28, Phase 3 code, commit `6fdce90` | [docs/phase-logs/phase3.md](docs/phase-logs/phase3.md), DESIGN.md section 12.5; section 4 | the "Fault-injection scenarios" step of every CI run since |
| C | `tools/dev.sh tools/run-system-tests.sh --coverage` | 2026-10-29, Phase 5 code, commit `fd9f17d` | sections 2.1 to 2.3 | section 2.4, the same suite on the runner in CI run 40826567803 (commit `aa1c495`) |
| D | `tools/dev.sh tools/run-system-tests.sh --results build/test-results-rust --sensor-exe rust/target/release/sensor-vcan` | 2026-10-29, Phase 7 code, commit `8472b9b` | section 3.1 | section 3.2, the same suite with the Rust sensor on the runner in CI run 40826567803 |
| E | `tools/dev.sh tools/static-analysis.sh` | 2026-10-29, Phase 6 code, commit `8d34d3e` | [docs/phase-logs/phase6.md](docs/phase-logs/phase6.md) section 3 | the "Static analysis" step of every CI run since |
| F | `tools/dev.sh tools/build-rust.sh` | 2026-10-29, Phase 7 code, commit `8472b9b` | [docs/phase-logs/phase7.md](docs/phase-logs/phase7.md) section 2 | the "Rust sensor ECU" step of every CI run since |
| G | the CI job itself, `.github/workflows/ci.yml` on a push to `main` | 2026-10-29, commit `aa1c495` | [docs/phase-logs/phase7.md](docs/phase-logs/phase7.md) section 5 | every later run |
| H | `tools/dev.sh tools/record-demo.sh --publish` (the demo, recorded) | 2026-10-30, the Phase 8 addendum commit | `docs/figures/demo.gif`; its capture drives `docs/figures/fault-timeline.svg` | the "Demo recording" step of every CI run since, uploaded as `demo-and-figures` |

The figures of the README (`docs/figures/*.svg`, drawn by `tools/dev.sh python3
tools/plots.py`; each footer names its capture): `fault-timeline.svg` from run
H's capture; `watchdog-timeline.svg` and `period-histograms.svg` from the
`test_heartbeat_loss` and `test_periodic_timing` captures of the local results
directories, the C++ one a run of the suite on 2026-10-29 at 05:59 UTC (the
verification run of the harness fix, commit `aa1c495`) and the Rust one run D;
`echo-rtt.svg` from the local `record_and_analyze.py` run of 2026-10-29 at
01:04 UTC (2040 frames, the Phase 6 code). CI redraws all four from its own
run's captures (`demo-and-figures` artifact).

Every row of the README table, in its order:

| README row | Raw data | Run |
|---|---|---|
| Bus load of the periodic traffic at 500 kbit/s | section 1.1, the analysis block | A |
| SENSOR_DATA period, nominal 10 ms | section 1.1 (1.2 in parentheses) | A |
| CONTROLLER_COMMAND period, nominal 20 ms | section 1.1 (1.2 in parentheses) | A |
| Echo round trip, host → ECU → host | section 1.1 (1.2 in parentheses), 1.3 for where the time goes | A |
| Gateway relay latency, no faults | section 4, phase3.md (the `baseline` scenario) | B |
| Missing SENSOR_DATA detected (timeout 35 ms) | section 2.1, `test_missing_message_is_detected_within_its_timeout[SENSOR_DATA]` | C |
| Missing SENSOR_HEARTBEAT detected (timeout 350 ms) | section 2.1, `...[SENSOR_HEARTBEAT]` | C |
| Missing BATTERY_STATUS detected (timeout 350 ms, DEGRADED) | section 2.1, `...[BATTERY_STATUS]` | C |
| Missing CONTROLLER_COMMAND detected by the sensor (timeout 70 ms) | section 2.1, `...[CONTROLLER_COMMAND]` | C |
| Safe state when the sensor heartbeat is lost | section 2.1, `test_heartbeat_loss_drives_the_controller_into_safe_state` | C |
| Safe state when the sensor process dies, recovery after its restart | section 2.1, `test_sensor_process_death_and_restart` | C |
| Stalled transmit thread, watchdog reset | section 2.1, `test_stalled_thread_is_reset_by_the_watchdog` | C |
| Error-passive entry and healing | section 2.1, `test_error_passive_entry_and_healing` | C |
| Bus-off recovery of the simulated controller | section 2.1, `test_bus_off_recovery_after_the_backoff` | C |
| Repeated bus-off: backoff doubling, FAULT latch, `ecu reset` | section 2.1, `test_repeated_bus_off_doubles_the_backoff_until_fault` | C |
| Flood of 2000 unknown frames/s on the controller's segment | section 2.1, `test_unknown_frame_flood_is_counted_and_not_disruptive` | C |
| Flood sweep 1000 / 2000 / 4000 frames/s | section 2.1, `test_flood_rate_sweep` | C |
| Corrupted frames rejected by CRC | section 2.1, `test_corrupted_frames_are_rejected_by_crc` | C |
| CRC fault published in the heartbeat flags | section 2.1, `test_crc_fault_is_latched_in_the_heartbeat_flags` | C |
| Stale counter and duplicated frames rejected | section 2.1, `test_stale_counter_is_rejected_as_repeated` | C |
| Random loss, 10 % both ways for 3 s, accounted for | section 2.1, `test_dropped_frames_are_accounted_by_the_counters` | C |
| Latency 15 ± 5 ms and a constant 20 ms: no loss | section 2.1, `test_delayed_frames_are_not_losses` | C |
| Reordered SENSOR_DATA rejected out of sequence | section 2.1, `test_reordered_frames_are_rejected_out_of_sequence` | C |
| SENSOR_DATA period under the suite (10 ms) | section 2.1, `test_periodic_messages_stay_within_tolerance` | C |
| Firmware clock against bus time | section 2.1, `test_firmware_clock_tracks_bus_time` | C |
| Line coverage | sections 2.2 and 2.3 (the runner's figures in 2.4) | C |
| Static analysis | phase6.md section 3 | E |
| The unchanged suite with the Rust sensor ECU in the sensor's place | section 3.1 (the controller's reactions in `...[SENSOR_DATA]`, the sensor's in `...[CONTROLLER_COMMAND]`) | D |
| Rust sensor: SENSOR_DATA period (10 ms) and clock | section 3.1, `test_periodic_messages_stay_within_tolerance`, `test_firmware_clock_tracks_bus_time` | D |
| Rust sensor: stalled transmit thread, software watchdog | section 3.1, `test_stalled_thread_is_reset_by_the_watchdog` | D |
| Rust sensor: killed and restarted | section 3.1, `test_sensor_process_death_and_restart` | D |
| Rust gate | phase7.md section 2 | F |
| CI, the whole pipeline on a GitHub-hosted runner | phase7.md section 5 | G |

The CI runs of the project, one per push to `main` (the Dependabot runs the
repository also records, and the runs of the documentation-only commits that
followed a phase, are not listed):

| Commit | Run | Result | Steps | Duration |
|---|---|---|---|---|
| `6fdce90`, Phases 0 to 3 pushed together when the repository was created | [40795735230](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40795735230) | failed in the allocator check: `grep -q` on a pipe under `pipefail`, a SIGPIPE race that made a healthy image look symbol-less (phase4.md section 6) | 25 | |
| `a3b7467`, Phase 4 | [40799295827](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40799295827) | passed | 25 | 6 min 10 s |
| `fd9f17d`, Phase 5 | [40813161893](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40813161893) | passed | 27 | 8 min 0 s |
| `8d34d3e`, Phase 6 | [40818484542](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40818484542) | passed | 30 | 14 min 9 s (image layers rebuilt) |
| `8472b9b`, Phase 7 | [40824884673](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40824884673) | failed in the C++ system suite: a floating-point difference of two timestamps compared against a limit it equalled (phase7.md section 5) | 33 | 11 min 10 s |
| `aa1c495`, the harness fix | [40826567803](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40826567803) | passed | 33 | 12 min 12 s |
| `1d54d38`, Phase 8 | [40901551354](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40901551354) | passed | 33 | 10 min 39 s |
| `9163adc`, the demo and the figures | [41024544832](https://github.com/dcgursoy/can-ecu-sim/actions/runs/41024544832) | passed | 35 | 14 min 37 s (a new image layer) |

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

### 2.4 The same suite on a GitHub-hosted runner

The `System test suite on vcan` step of CI run
[40826567803](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40826567803)
(commit `aa1c495`, `ubuntu-24.04`, the same container image, the same
command with `--coverage`): 23 of 23 passed, 84.5 s of test time
(2026-10-29T06:10:36+00:00), firmware coverage 90 % of lines (942 of 1040) and the
library headers 100 % (937 of 937). The firmware's own clocks give the same
numbers as on the desktop; the host-side ones differ by the runner's
scheduling. Side by side with the local run above:

| Measurement | Local run (quoted in the README) | GitHub-hosted runner |
|---|---|---|
| SENSOR_DATA period std | 0.109 ms | 0.059 ms |
| SENSOR_DATA period p99 | 10.3 ms | 10.1 ms |
| SENSOR_DATA period max | 10.5 ms | 10.2 ms |
| Sensor heartbeat clock, largest deviation from bus time | 0.841 ms | 0.828 ms |
| Missing SENSOR_DATA: controller reaction | 51.3 ms | 59.6 ms |
| Missing SENSOR_HEARTBEAT: controller reaction | 371.0 ms | 359.1 ms |
| Missing BATTERY_STATUS: controller reaction | 421.5 ms | 429.8 ms |
| Missing CONTROLLER_COMMAND: sensor reaction | 168.7 ms | 80.5 ms |
| Heartbeat loss: SAFE_STATE after the last heartbeat | 374.8 ms | 359.0 ms |
| Sensor killed: SAFE_STATE after its last SENSOR_DATA | 50.0 ms | 49.7 ms |
| Sensor restarted: NORMAL after its first heartbeat | 507.5 ms | 520.5 ms |
| Stalled TX thread: feed withheld after the stall | 295.0 ms | 295.0 ms |
| Stalled TX thread: watchdog reset after the last frame | 1301.6 ms | 1297.0 ms |
| Stalled TX thread: reboot to the first frame | 11.6 ms | 14.4 ms |
| Time in error-passive (firmware log) | 530.0 ms | 535.0 ms |
| Bus-off to error-active (firmware log) | 105.0 ms | 105.0 ms |
| Bus-off: CONTROLLER_COMMAND silence | 140.0 ms | 120.0 ms |
| Corrupted frames rejected | 22 rejected, 22 corrupted of 422 | 22 rejected, 22 corrupted of 422 |
| Reordered frames rejected | 31 | 31 |
| Flood 2000/s: CONTROLLER_COMMAND period max | 20.7 ms | 20.1 ms |

The runner's `measurements.md`, from the `test-results` artifact:

| test | measurement | value | limit | result |
|---|---|---|---|---|
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood rate achieved by the gateway (2000/s requested) (4001 frames in 2.001 s, 0 send errors; Python sender on a desktop OS) | 1999.9 frames/s | >= 1800.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood frames counted as unknown by the controller (4001 of 4001) | 100.0 % | >= 90.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | no frame dropped by a full receive queue | 0 dropped, queue high-water mark 5/32 | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | receive queue high-water mark / depth after the flood | 5/32 frames |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 during the flood (n=94, max 20.1 ms) | 20.1 ms | 18.0..22.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period max during the flood | 20.1 ms | <= 25.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 after the flood (n=21, max 20.1 ms) | 20.1 ms |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | SENSOR_DATA accepted by the controller during the experiment | 99.9 frames/s | 95.0..105.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | controller: no timeouts, no CRC or counter faults, no safe state, RUNNING |  | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | the flood stayed on its segment: the sensor saw no unknown frames, is RUNNING |  | expected | ok |
| test_flood_rate_sweep | 1000/s requested: achieved (1001 frames) | 999.9 frames/s |  | info |
| test_flood_rate_sweep | 1000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 1000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 1000/s: CONTROLLER_COMMAND period max during the flood (p99 20.08 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 1000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 2000/s requested: achieved (2001 frames) | 1999.8 frames/s |  | info |
| test_flood_rate_sweep | 2000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 2000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 2000/s: CONTROLLER_COMMAND period max during the flood (p99 20.10 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 2000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 4000/s requested: achieved (4001 frames) | 3999.7 frames/s |  | info |
| test_flood_rate_sweep | 4000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 4000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 7/32 | expected | ok |
| test_flood_rate_sweep | 4000/s: CONTROLLER_COMMAND period max during the flood (p99 20.11 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 4000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_error_passive_entry_and_healing | ERROR_PASSIVE visible in the controller heartbeat after the injection (INJECT_CAN_ERROR x20: TEC 160) | 33.2 ms | 0..145.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (firmware log, ECU clock) (TEC heals by one per successful frame at 60/s: 33 frames, 550 ms expected) | 535.0 ms | 490.0..610.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (heartbeats, 100 ms granularity) | 599.5 ms |  | info |
| test_error_passive_entry_and_healing | CONTROLLER_COMMAND rate while error-passive (transmission continues) | 50.0 frames/s | 45.0..55.0 | ok |
| test_error_passive_entry_and_healing | one passive event, no bus-off, no safe state (error-passive is not an input loss) |  | expected | ok |
| test_error_passive_entry_and_healing | controller ERROR_ACTIVE and RUNNING again, TEC healed | state ERROR_ACTIVE tec=70 | expected | ok |
| test_bus_off_recovery_after_the_backoff | exactly one gap in the controller's commands | 1 | expected | ok |
| test_bus_off_recovery_after_the_backoff | CONTROLLER_COMMAND silence on the bus (bus-off, backoff, recovery) (backoff 100 ms + modelled recovery 3 ms + supervisor tick + next TX slot) | 120.0 ms | 100..168.0 | ok |
| test_bus_off_recovery_after_the_backoff | bus-off to error-active in the firmware log (ECU clock) | 105.0 ms | 100..118 | ok |
| test_bus_off_recovery_after_the_backoff | last_recovery_ms reported by `ecu status` | 105 ms |  | info |
| test_bus_off_recovery_after_the_backoff | one bus-off, one attempt, one recovery ok, no failure, phase active | phase active | expected | ok |
| test_bus_off_recovery_after_the_backoff | bus-off is an input loss: one safe-state entry, the first command after the gap is SAFE_STATE | first mode SAFE_STATE | expected | ok |
| test_bus_off_recovery_after_the_backoff | NORMAL again after the first command following recovery (recovery hold 500 ms) | 500.0 ms | 496..565.0 | ok |
| test_bus_off_recovery_after_the_backoff | heartbeat after recovery: BUS_OFF_SEEN flag set, bus_off_count 1 |  | expected | ok |
| test_bus_off_recovery_after_the_backoff | sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again | [00:00:02.705,000] <inf> ecu: node_state: RUNNING -> DEGRADED (critical input lost); [00:00:02.750,000] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal) | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #1: command silence (backoff 100 ms) | 140.0 ms | 100..168.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #2: command silence (backoff 200 ms (doubled: the previous recovery did not hold)) | 220.0 ms | 200..268.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #3: command silence (backoff 400 ms (doubled: the previous recovery did not hold)) | 420.0 ms | 400..468.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #4: command silence (backoff 800 ms (doubled: the previous recovery did not hold)) | 820.0 ms | 800..868.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #5: command silence (backoff 1600 ms (doubled: the previous recovery did not hold)) | 1620.1 ms | 1600..1668.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | every attempted recovery completed before the next bus-off (5) | [True, True, True, True, True] | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #6: no recovery, phase failed, node FAULT, transmission stopped | phase failed, state FAULT, 0 commands after it | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | 5 attempts, 5 recoveries ok, 5 failures (the attempts are exhausted) | attempts 5, ok 5, failures 5 | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | first command after `ecu reset` (the reset restarts the backoff from the first attempt; the controller is still bus-off) | 118.0 ms | 100..188.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | NORMAL again after `ecu reset` (backoff 100 ms + recovery hold 500 ms) | 617.9 ms | 600..693.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | controller RUNNING, CAN phase active after the reset | state RUNNING, phase active | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | sensor: one CONTROLLER_COMMAND timeout per silence (6), RUNNING again | 6 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller CRC rejections == frames the gateway corrupted | 22 rejected, 22 corrupted of 422 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: runs of consecutive corrupted frames (longest 2; a run of rejects is silence to the 35 ms timeout) | 17 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 3 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 1 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller safe-state entries == timeout runs on critical inputs (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller RUNNING again; sensor unaffected |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | sensor corrupted 2 frames, controller rejected 2 by CRC | 2 corrupted, 2 bad on the bus, 2 rejected | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN clear in the heartbeat before the fault (RESET_FAULTS) |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN published in the controller heartbeat after the first bad frame (flag set on rejection, published by the next heartbeat) | 40.4 ms | 0..145.0 | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | two bad frames are tolerated: no timeout, no safe state |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | sensor repeated the previous counter on 2 frames, controller rejected 2 as repeated | 2 frozen, 2 stale on the bus, 2 rejected | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN clear before the fault |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN published after the first stale frame | 50.5 ms | 0..145.0 | ok |
| test_stale_counter_is_rejected_as_repeated | two stale frames are tolerated: no CRC fault, no timeout, no safe state | crc 0, timeouts 0, safe-state entries 0 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | controller repeated-counter rejections == frames the gateway duplicated | 41 rejected, 41 duplicated of 365 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | duplicates cause no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | sensor skipped 3 counter values: controller accepted 3 frames with loss, 3 frames counted lost | 3 skipped, 3 accepted with loss, 3 lost | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | nothing rejected (the data is fresh), no timeout, no safe state |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | SENSOR_DATA frames dropped for 15 ms (largest gap on the bus 30.1 ms) | 2 frames |  | info |
| test_skipped_counter_is_accepted_and_the_loss_counted | timeouts == gaps over 35 ms (0) | 0 | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | dropped == counted lost by the counter + frames inside timeout gaps | 2 dropped, 2 lost, 0 in timeout gaps | expected | ok |
| test_persistent_bad_frames_degrade_the_node | CRC rejections == corrupted BATTERY_STATUS frames (at least 3) | 3 rejected, 3 corrupted | expected | ok |
| test_persistent_bad_frames_degrade_the_node | DEGRADED in the heartbeat after the 3rd consecutive bad frame | 29.4 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | RUNNING again after the first good frame | 29.3 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | no safe state (battery is not a critical input), SENSOR_DATA untouched |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped by the gateway (controller counted 32 lost by counter and 0 timeouts; 0 gaps over 35 ms hold 0 frames) | 32 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped == counted lost + frames inside timeout gaps | 32 vs 32 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped by the gateway (controller counted 1 lost by counter and 0 timeouts; 0 gaps over 350 ms hold 0 frames) | 1 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped == counted lost + frames inside timeout gaps | 1 vs 1 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped by the gateway (sensor counted 15 lost by counter and 0 timeouts; 0 gaps over 70 ms hold 0 frames) | 15 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped == counted lost + frames inside timeout gaps | 15 vs 15 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | controller safe-state entries == loss events on its critical inputs (0) | 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | no CRC faults on either side (loss is not corruption); both RUNNING again |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | gateway: dropped sensor->controller / controller->sensor | 41 of 554 / 21 of 277 |  | info |
| test_duplicated_frames_are_rejected | controller repeated-counter rejections == frames the gateway duplicated | 34 rejected, 34 duplicated of 363 | expected | ok |
| test_duplicated_frames_are_rejected | COUNTER_FAULT_SEEN clear before, published in the controller heartbeat after |  | expected | ok |
| test_duplicated_frames_are_rejected | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p1 with 15 +/- 5 ms configured (n=285) | 10.4 ms | >= 9.500 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency median | 15.4 ms | 13.0..17.0 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p99 | 20.0 ms | <= 23.0 | ok |
| test_delayed_frames_are_not_losses | gateway max scheduled latency a->b / b->a | 19.98 / 19.98 ms |  | info |
| test_delayed_frames_are_not_losses | latency is not loss for a period-based timeout: no timeouts, no safe state, both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: SENSOR_DATA latency median | 20.1 ms | 19.0..22.0 | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: largest SENSOR_DATA gap on the controller's segment (the step into the delay) (period 10 ms + 20 ms step, below the 35 ms timeout) | 30.0 ms | <= 34 | ok |
| test_delayed_frames_are_not_losses | 20 ms of latency: no timeout, no counter fault, no safe state |  | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | SENSOR_DATA frames swapped with their successor (0 held frames released late without a successor) | 31 frames |  | info |
| test_reordered_frames_are_rejected_out_of_sequence | out-of-sequence rejections == swaps (the late frame is rejected) | 31 | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | accepted-with-loss events == swaps (the successor arrives with counter +2), one frame counted lost each | 31 accepted with loss, 31 lost | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_sensor_process_death_and_restart | SAFE_STATE command after the last SENSOR_DATA of the killed sensor (the 35 ms SENSOR_DATA timeout fires first; the heartbeat timeout would take 350 ms) | 49.7 ms | 31..100.0 | ok |
| test_sensor_process_death_and_restart | time from the kill to the SAFE_STATE command | 42.6 ms |  | info |
| test_sensor_process_death_and_restart | controller in SAFE_STATE while the sensor is dead | SAFE_STATE | expected | ok |
| test_sensor_process_death_and_restart | restarted sensor: first frame after the process start (process start, Zephyr boot, first TX tick) | 11.1 ms | <= 1000.0 | ok |
| test_sensor_process_death_and_restart | restarted sensor: first heartbeat after its first frame | 60.4 ms | 0..110.0 | ok |
| test_sensor_process_death_and_restart | NORMAL command after the first heartbeat of the restarted sensor (both critical inputs alive from the heartbeat on; hold 500 ms) | 520.5 ms | 496..565.0 | ok |
| test_sensor_process_death_and_restart | controller: one timeout per sensor message, one safe-state entry | {'SENSOR_DATA': 1, 'BATTERY_STATUS': 1, 'SENSOR_HEARTBEAT': 1} | expected | ok |
| test_sensor_process_death_and_restart | the restarted sensor's counters (from 0) are accepted: no counter rejections, no CRC faults (the timeout reset the receiver's counter history) |  | expected | ok |
| test_sensor_process_death_and_restart | both RUNNING at the end |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | exactly one gap in SENSOR_DATA | 1 | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | SENSOR_DATA silence on the bus (stall, watchdog reset, reboot, first TX tick) | 1311.4 ms |  | info |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog reset after the last frame (firmware uptime dates the reboot) (the supervisor feeds the watchdog until the stalled thread is 300 ms stale, then the watchdog's 1000 ms run out) | 1297.0 ms | 1290..1330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | reboot to the first frame of the rebooted sensor (host: exec, Zephyr boot, first TX tick) (host time, not firmware time: the executable is re-executed from the bind mount) | 14.4 ms | <= 1500.0 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog feed withheld after the liveness window (firmware log, ECU clock) (liveness window 300 ms) | 295.0 ms | 290..330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor rebooted in place: boot banner again, uptime restarted, same process | uptime 1710 ms, same pid | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | controller: one safe-state entry, RUNNING again |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor RUNNING again |  | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | heartbeats suppressed by the sensor (SUPPRESS_HEARTBEAT 600 ms) | 6 frames |  | info |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SENSOR_HEARTBEAT silence on the bus (600 ms of suppression starts and ends between two 100 ms heartbeat slots) | 700.6 ms | 590..810.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SAFE_STATE command after the last heartbeat (heartbeat timeout 350 ms + supervisor tick + command period) | 359.0 ms | 346..415.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | NORMAL command after the first heartbeat again (recovery hold 500 ms + tick + command period) | 518.4 ms | 496..565.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | every command in the safe state: actuators off, brake hold, speed limit 0, torque 0 | 43 commands over 860 ms | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, SENSOR_DATA never timed out, RUNNING again |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: frames dropped by the gateway (435 ms of loss, period 10 ms) | 44 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 35 ms + supervisor tick 5 ms + 20 ms publish period) | 59.6 ms | 31..100.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 509.7 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_DATA, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: frames dropped by the gateway (750 ms of loss, period 100 ms) | 8 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 20 ms publish period) | 359.1 ms | 346..415.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 519.5 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_HEARTBEAT, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: frames dropped by the gateway (750 ms of loss, period 100 ms) | 7 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller reaction (CONTROLLER_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 100 ms publish period) | 429.8 ms | 346..495.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 29.4 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: exactly one timeout counted by the controller, none for its other inputs | 1 for BATTERY_STATUS, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: frames dropped by the gateway (470 ms of loss, period 20 ms) | 24 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor reaction (SENSOR_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 70 ms + supervisor tick 5 ms + 100 ms publish period) | 80.5 ms | 66..215.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 81.0 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: exactly one timeout counted by the sensor, none for its other inputs | 1 for CONTROLLER_COMMAND, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: controller unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period mean (n=499, nominal 10 ms) | 10.0 ms | 9.900..10.1 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period std | 0.059 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period p99 | 10.1 ms | 9.000..11.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period min | 9.860 ms | >= 7.500 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period max | 10.2 ms | <= 12.5 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA rate | 100.0 frames/s | 98.0..102.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period std | 0.317 ms |  | info |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period p99 | 100.2 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period min | 98.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period max | 100.2 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS rate | 9.999 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period std | 0.334 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period p99 | 100.2 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period min | 98.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period max | 100.2 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period mean (n=249, nominal 20 ms) | 20.0 ms | 19.8..20.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period std | 0.043 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period p99 | 20.1 ms | 18.0..22.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period min | 19.9 ms | >= 15.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period max | 20.1 ms | <= 25.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND rate | 50.0 frames/s | 49.0..51.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period std | 0.335 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period p99 | 100.2 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period min | 98.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period max | 100.2 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | sensor: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | sensor: watchdog fed on every supervisor tick | feeds 1110, skipped 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: watchdog fed on every supervisor tick | feeds 1112, skipped 0 | expected | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.7 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.828 ms | <= 15.0 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.6 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.923 ms | <= 15.0 | ok |

## 3. Phase 7: the same suite with the Rust sensor ECU in the sensor's place

Command: `tools/dev.sh tools/run-system-tests.sh --results build/test-results-rust
--sensor-exe rust/target/release/sensor-vcan` on 2026-10-29 (the run quoted in
the README and in DESIGN.md section 16.7): 23 of 23 tests passed in 86.5 s,
seed 1, no attempt inconclusive. The sensor on `vcan0` was the Rust
implementation (`rust/`, the `no_std` crate `sensor-core` under the SocketCAN
adapter `sensor-vcan`, the release build before the three review fixes of
[docs/phase-logs/phase7.md](docs/phase-logs/phase7.md) section 4, which touch
paths the suite does not exercise; the final code of the commit passed the
same run again, 23 of 23 in 84.7 s);
the controller on `vcan1` and the expectations (from its `.config`) were the
C++ firmware's, the gateway and the test code were unchanged apart from one
needle (the watchdog test looks for `*** Booting` instead of Zephyr's
`*** Booting Zephyr`, DESIGN.md section 16.4). The limits read as in section
2. `build/test-results-rust/` holds the same files as `build/test-results/`
(JUnit, `measurements.json`, this table, the logs of every module), and CI
uploads it as the `test-results-rust` artifact.

Before the run, `tools/dev.sh tools/build-rust.sh` on the same commit: rustfmt
and clippy (pedantic, every warning an error) clean; 34 unit tests of
`sensor-core` and 6 conformance tests against the cantools / `e2e_ref.py`
vectors passed; `sensor-vcan` built in release (6.2 MB with debug info,
513 KB stripped); `sensor-core` cross-compiled for `thumbv7em-none-eabihf`
(`libsensor_core.rlib`, 867 KB with metadata). Toolchain: rustc and cargo
1.89.0, `socketcan` 3.6.2, `libm` 0.2.16, `libc` 0.2.191 (`rust/Cargo.lock`).

### 3.1 Measured values, `measurements.md` of the run

| test | measurement | value | limit | result |
|---|---|---|---|---|
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood rate achieved by the gateway (2000/s requested) (4001 frames in 2.001 s, 0 send errors; Python sender on a desktop OS) | 1999.9 frames/s | >= 1800.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood frames counted as unknown by the controller (4001 of 4001) | 100.0 % | >= 90.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | no frame dropped by a full receive queue | 0 dropped, queue high-water mark 5/32 | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | receive queue high-water mark / depth after the flood | 5/32 frames |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 during the flood (n=94, max 20.2 ms) | 20.2 ms | 18.0..22.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period max during the flood | 20.2 ms | <= 25.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 after the flood (n=21, max 20.9 ms) | 20.7 ms |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | SENSOR_DATA accepted by the controller during the experiment | 100.3 frames/s | 95.0..105.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | controller: no timeouts, no CRC or counter faults, no safe state, RUNNING |  | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | the flood stayed on its segment: the sensor saw no unknown frames, is RUNNING |  | expected | ok |
| test_flood_rate_sweep | 1000/s requested: achieved (1001 frames) | 999.9 frames/s |  | info |
| test_flood_rate_sweep | 1000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 1000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 1000/s: CONTROLLER_COMMAND period max during the flood (p99 20.27 ms) | 20.3 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 1000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 2000/s requested: achieved (2001 frames) | 1999.9 frames/s |  | info |
| test_flood_rate_sweep | 2000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 2000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 2000/s: CONTROLLER_COMMAND period max during the flood (p99 20.11 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 2000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 4000/s requested: achieved (4001 frames) | 3999.6 frames/s |  | info |
| test_flood_rate_sweep | 4000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 4000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 7/32 | expected | ok |
| test_flood_rate_sweep | 4000/s: CONTROLLER_COMMAND period max during the flood (p99 20.17 ms) | 20.2 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 4000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_error_passive_entry_and_healing | ERROR_PASSIVE visible in the controller heartbeat after the injection (INJECT_CAN_ERROR x20: TEC 160) | 58.8 ms | 0..145.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (firmware log, ECU clock) (TEC heals by one per successful frame at 60/s: 33 frames, 550 ms expected) | 560.0 ms | 490.0..610.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (heartbeats, 100 ms granularity) | 599.6 ms |  | info |
| test_error_passive_entry_and_healing | CONTROLLER_COMMAND rate while error-passive (transmission continues) | 50.0 frames/s | 45.0..55.0 | ok |
| test_error_passive_entry_and_healing | one passive event, no bus-off, no safe state (error-passive is not an input loss) |  | expected | ok |
| test_error_passive_entry_and_healing | controller ERROR_ACTIVE and RUNNING again, TEC healed | state ERROR_ACTIVE tec=70 | expected | ok |
| test_bus_off_recovery_after_the_backoff | exactly one gap in the controller's commands | 1 | expected | ok |
| test_bus_off_recovery_after_the_backoff | CONTROLLER_COMMAND silence on the bus (bus-off, backoff, recovery) (backoff 100 ms + modelled recovery 3 ms + supervisor tick + next TX slot) | 119.9 ms | 100..168.0 | ok |
| test_bus_off_recovery_after_the_backoff | bus-off to error-active in the firmware log (ECU clock) | 100.0 ms | 100..118 | ok |
| test_bus_off_recovery_after_the_backoff | last_recovery_ms reported by `ecu status` | 103 ms |  | info |
| test_bus_off_recovery_after_the_backoff | one bus-off, one attempt, one recovery ok, no failure, phase active | phase active | expected | ok |
| test_bus_off_recovery_after_the_backoff | bus-off is an input loss: one safe-state entry, the first command after the gap is SAFE_STATE | first mode SAFE_STATE | expected | ok |
| test_bus_off_recovery_after_the_backoff | NORMAL again after the first command following recovery (recovery hold 500 ms) | 500.0 ms | 496..565.0 | ok |
| test_bus_off_recovery_after_the_backoff | heartbeat after recovery: BUS_OFF_SEEN flag set, bus_off_count 1 |  | expected | ok |
| test_bus_off_recovery_after_the_backoff | sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again | [00:00:02.635,944] <inf> ecu: node_state: RUNNING -> DEGRADED (critical input lost); [00:00:02.685,955] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal) | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #1: command silence (backoff 100 ms) | 120.1 ms | 100..168.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #2: command silence (backoff 200 ms (doubled: the previous recovery did not hold)) | 220.0 ms | 200..268.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #3: command silence (backoff 400 ms (doubled: the previous recovery did not hold)) | 440.3 ms | 400..468.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #4: command silence (backoff 800 ms (doubled: the previous recovery did not hold)) | 839.7 ms | 800..868.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #5: command silence (backoff 1600 ms (doubled: the previous recovery did not hold)) | 1640.1 ms | 1600..1668.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | every attempted recovery completed before the next bus-off (5) | [True, True, True, True, True] | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #6: no recovery, phase failed, node FAULT, transmission stopped | phase failed, state FAULT, 0 commands after it | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | 5 attempts, 5 recoveries ok, 5 failures (the attempts are exhausted) | attempts 5, ok 5, failures 5 | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | first command after `ecu reset` (the reset restarts the backoff from the first attempt; the controller is still bus-off) | 123.7 ms | 100..188.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | NORMAL again after `ecu reset` (backoff 100 ms + recovery hold 500 ms) | 623.7 ms | 600..693.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | controller RUNNING, CAN phase active after the reset | state RUNNING, phase active | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | sensor: one CONTROLLER_COMMAND timeout per silence (6), RUNNING again | 6 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller CRC rejections == frames the gateway corrupted | 22 rejected, 22 corrupted of 422 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: runs of consecutive corrupted frames (longest 2; a run of rejects is silence to the 35 ms timeout) | 16 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 4 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 1 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller safe-state entries == timeout runs on critical inputs (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller RUNNING again; sensor unaffected |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | sensor corrupted 2 frames, controller rejected 2 by CRC | 2 corrupted, 2 bad on the bus, 2 rejected | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN clear in the heartbeat before the fault (RESET_FAULTS) |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN published in the controller heartbeat after the first bad frame (flag set on rejection, published by the next heartbeat) | 93.7 ms | 0..145.0 | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | two bad frames are tolerated: no timeout, no safe state |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | sensor repeated the previous counter on 2 frames, controller rejected 2 as repeated | 2 frozen, 2 stale on the bus, 2 rejected | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN clear before the fault |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN published after the first stale frame | 23.7 ms | 0..145.0 | ok |
| test_stale_counter_is_rejected_as_repeated | two stale frames are tolerated: no CRC fault, no timeout, no safe state | crc 0, timeouts 0, safe-state entries 0 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | controller repeated-counter rejections == frames the gateway duplicated | 41 rejected, 41 duplicated of 362 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | duplicates cause no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | sensor skipped 3 counter values: controller accepted 3 frames with loss, 3 frames counted lost | 3 skipped, 3 accepted with loss, 3 lost | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | nothing rejected (the data is fresh), no timeout, no safe state |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | SENSOR_DATA frames dropped for 15 ms (largest gap on the bus 30.1 ms) | 2 frames |  | info |
| test_skipped_counter_is_accepted_and_the_loss_counted | timeouts == gaps over 35 ms (0) | 0 | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | dropped == counted lost by the counter + frames inside timeout gaps | 2 dropped, 2 lost, 0 in timeout gaps | expected | ok |
| test_persistent_bad_frames_degrade_the_node | CRC rejections == corrupted BATTERY_STATUS frames (at least 3) | 4 rejected, 4 corrupted | expected | ok |
| test_persistent_bad_frames_degrade_the_node | DEGRADED in the heartbeat after the 3rd consecutive bad frame | 23.7 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | RUNNING again after the first good frame | 24.1 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | no safe state (battery is not a critical input), SENSOR_DATA untouched |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped by the gateway (controller counted 31 lost by counter and 0 timeouts; 0 gaps over 35 ms hold 0 frames) | 31 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped == counted lost + frames inside timeout gaps | 31 vs 31 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped by the gateway (controller counted 5 lost by counter and 0 timeouts; 0 gaps over 350 ms hold 0 frames) | 5 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped == counted lost + frames inside timeout gaps | 5 vs 5 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped by the gateway (sensor counted 15 lost by counter and 0 timeouts; 0 gaps over 70 ms hold 0 frames) | 15 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped == counted lost + frames inside timeout gaps | 15 vs 15 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | controller safe-state entries == loss events on its critical inputs (0) | 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | no CRC faults on either side (loss is not corruption); both RUNNING again |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | gateway: dropped sensor->controller / controller->sensor | 41 of 555 / 21 of 278 |  | info |
| test_duplicated_frames_are_rejected | controller repeated-counter rejections == frames the gateway duplicated | 34 rejected, 34 duplicated of 362 | expected | ok |
| test_duplicated_frames_are_rejected | COUNTER_FAULT_SEEN clear before, published in the controller heartbeat after |  | expected | ok |
| test_duplicated_frames_are_rejected | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p1 with 15 +/- 5 ms configured (n=285) | 10.4 ms | >= 9.500 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency median | 15.6 ms | 13.0..17.0 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p99 | 20.1 ms | <= 23.0 | ok |
| test_delayed_frames_are_not_losses | gateway max scheduled latency a->b / b->a | 19.98 / 19.98 ms |  | info |
| test_delayed_frames_are_not_losses | latency is not loss for a period-based timeout: no timeouts, no safe state, both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: SENSOR_DATA latency median | 20.2 ms | 19.0..22.0 | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: largest SENSOR_DATA gap on the controller's segment (the step into the delay) (period 10 ms + 20 ms step, below the 35 ms timeout) | 30.1 ms | <= 34 | ok |
| test_delayed_frames_are_not_losses | 20 ms of latency: no timeout, no counter fault, no safe state |  | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | SENSOR_DATA frames swapped with their successor (0 held frames released late without a successor) | 31 frames |  | info |
| test_reordered_frames_are_rejected_out_of_sequence | out-of-sequence rejections == swaps (the late frame is rejected) | 31 | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | accepted-with-loss events == swaps (the successor arrives with counter +2), one frame counted lost each | 31 accepted with loss, 31 lost | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_sensor_process_death_and_restart | SAFE_STATE command after the last SENSOR_DATA of the killed sensor (the 35 ms SENSOR_DATA timeout fires first; the heartbeat timeout would take 350 ms) | 44.5 ms | 31..100.0 | ok |
| test_sensor_process_death_and_restart | time from the kill to the SAFE_STATE command | 40.4 ms |  | info |
| test_sensor_process_death_and_restart | controller in SAFE_STATE while the sensor is dead | SAFE_STATE | expected | ok |
| test_sensor_process_death_and_restart | restarted sensor: first frame after the process start (process start, Zephyr boot, first TX tick) | 15.5 ms | <= 1000.0 | ok |
| test_sensor_process_death_and_restart | restarted sensor: first heartbeat after its first frame | 59.9 ms | 0..110.0 | ok |
| test_sensor_process_death_and_restart | NORMAL command after the first heartbeat of the restarted sensor (both critical inputs alive from the heartbeat on; hold 500 ms) | 521.5 ms | 496..565.0 | ok |
| test_sensor_process_death_and_restart | controller: one timeout per sensor message, one safe-state entry | {'SENSOR_DATA': 1, 'BATTERY_STATUS': 1, 'SENSOR_HEARTBEAT': 1} | expected | ok |
| test_sensor_process_death_and_restart | the restarted sensor's counters (from 0) are accepted: no counter rejections, no CRC faults (the timeout reset the receiver's counter history) |  | expected | ok |
| test_sensor_process_death_and_restart | both RUNNING at the end |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | exactly one gap in SENSOR_DATA | 1 | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | SENSOR_DATA silence on the bus (stall, watchdog reset, reboot, first TX tick) | 1323.3 ms |  | info |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog reset after the last frame (firmware uptime dates the reboot) (the supervisor feeds the watchdog until the stalled thread is 300 ms stale, then the watchdog's 1000 ms run out) | 1313.7 ms | 1290..1330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | reboot to the first frame of the rebooted sensor (host: exec, Zephyr boot, first TX tick) (host time, not firmware time: the executable is re-executed from the bind mount) | 9.694 ms | <= 1500.0 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog feed withheld after the liveness window (firmware log, ECU clock) (liveness window 300 ms) | 305.1 ms | 290..330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor rebooted in place: boot banner again, uptime restarted, same process | uptime 1690 ms, same pid | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | controller: one safe-state entry, RUNNING again |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor RUNNING again |  | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | heartbeats suppressed by the sensor (SUPPRESS_HEARTBEAT 600 ms) | 6 frames |  | info |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SENSOR_HEARTBEAT silence on the bus (600 ms of suppression starts and ends between two 100 ms heartbeat slots) | 700.0 ms | 590..810.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SAFE_STATE command after the last heartbeat (heartbeat timeout 350 ms + supervisor tick + command period) | 358.8 ms | 346..415.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | NORMAL command after the first heartbeat again (recovery hold 500 ms + tick + command period) | 516.6 ms | 496..565.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | every command in the safe state: actuators off, brake hold, speed limit 0, torque 0 | 43 commands over 858 ms | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, SENSOR_DATA never timed out, RUNNING again |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: frames dropped by the gateway (435 ms of loss, period 10 ms) | 44 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 35 ms + supervisor tick 5 ms + 20 ms publish period) | 46.5 ms | 31..100.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 516.4 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_DATA, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: frames dropped by the gateway (750 ms of loss, period 100 ms) | 7 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 20 ms publish period) | 356.5 ms | 346..415.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 516.4 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_HEARTBEAT, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: frames dropped by the gateway (750 ms of loss, period 100 ms) | 8 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller reaction (CONTROLLER_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 100 ms publish period) | 427.5 ms | 346..495.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 26.9 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: exactly one timeout counted by the controller, none for its other inputs | 1 for BATTERY_STATUS, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: frames dropped by the gateway (470 ms of loss, period 20 ms) | 24 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor reaction (SENSOR_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 70 ms + supervisor tick 5 ms + 100 ms publish period) | 143.1 ms | 66..215.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 42.8 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: exactly one timeout counted by the sensor, none for its other inputs | 1 for CONTROLLER_COMMAND, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: controller unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period mean (n=499, nominal 10 ms) | 10.000 ms | 9.900..10.1 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period std | 0.050 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period p99 | 10.1 ms | 9.000..11.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period min | 9.598 ms | >= 7.500 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period max | 10.4 ms | <= 12.5 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA rate | 100.0 frames/s | 98.0..102.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period std | 0.034 ms |  | info |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period p99 | 100.1 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period min | 99.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period max | 100.1 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period std | 0.059 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period p99 | 100.1 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period min | 99.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period max | 100.2 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period mean (n=249, nominal 20 ms) | 20.0 ms | 19.8..20.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period std | 0.129 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period p99 | 20.4 ms | 18.0..22.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period min | 19.4 ms | >= 15.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period max | 20.6 ms | <= 25.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND rate | 50.0 frames/s | 49.0..51.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period std | 0.340 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period p99 | 100.4 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period min | 99.0 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period max | 100.5 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | sensor: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | sensor: watchdog fed on every supervisor tick | feeds 1100, skipped 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: watchdog fed on every supervisor tick | feeds 1106, skipped 0 | expected | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2900.0 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.060 ms | <= 15.0 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.9 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.810 ms | <= 15.0 | ok |

### 3.2 The same suite with the Rust sensor on a GitHub-hosted runner

The `System test suite against the Rust sensor ECU` step of the same CI run
([40826567803](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40826567803),
the binary built by the run's own Rust step): 23 of 23 passed,
84.0 s of test time (2026-10-29T06:12:02+00:00). Side by side with the
local run of section 3.1:

| Measurement | Local run (quoted in the README) | GitHub-hosted runner |
|---|---|---|
| SENSOR_DATA period std | 0.050 ms | 0.021 ms |
| SENSOR_DATA period p99 | 10.1 ms | 10.0 ms |
| SENSOR_DATA period max | 10.4 ms | 10.1 ms |
| Sensor heartbeat clock, largest deviation from bus time | 0.060 ms | 0.056 ms |
| Missing SENSOR_DATA: controller reaction | 46.5 ms | 49.3 ms |
| Missing SENSOR_HEARTBEAT: controller reaction | 356.5 ms | 359.3 ms |
| Missing BATTERY_STATUS: controller reaction | 427.5 ms | 430.3 ms |
| Missing CONTROLLER_COMMAND: sensor reaction | 143.1 ms | 140.6 ms |
| Heartbeat loss: SAFE_STATE after the last heartbeat | 358.8 ms | 369.6 ms |
| Sensor killed: SAFE_STATE after its last SENSOR_DATA | 44.5 ms | 59.4 ms |
| Sensor restarted: NORMAL after its first heartbeat | 521.5 ms | 512.1 ms |
| Stalled TX thread: feed withheld after the stall | 305.1 ms | 305.1 ms |
| Stalled TX thread: watchdog reset after the last frame | 1313.7 ms | 1312.4 ms |
| Stalled TX thread: reboot to the first frame | 9.694 ms | 10.2 ms |
| Time in error-passive (firmware log) | 560.0 ms | 545.0 ms |
| Bus-off to error-active (firmware log) | 100.0 ms | 105.0 ms |
| Bus-off: CONTROLLER_COMMAND silence | 119.9 ms | 139.9 ms |
| Corrupted frames rejected | 22 rejected, 22 corrupted of 422 | 22 rejected, 22 corrupted of 423 |
| Reordered frames rejected | 31 | 31 |
| Flood 2000/s: CONTROLLER_COMMAND period max | 20.2 ms | 20.1 ms |

The runner's `measurements.md`, from the `test-results-rust` artifact:

| test | measurement | value | limit | result |
|---|---|---|---|---|
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood rate achieved by the gateway (2000/s requested) (4001 frames in 2.001 s, 0 send errors; Python sender on a desktop OS) | 1999.9 frames/s | >= 1800.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | flood frames counted as unknown by the controller (4001 of 4001) | 100.0 % | >= 90.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | no frame dropped by a full receive queue | 0 dropped, queue high-water mark 5/32 | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | receive queue high-water mark / depth after the flood | 5/32 frames |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 during the flood (n=94, max 20.1 ms) | 20.1 ms | 18.0..22.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period max during the flood | 20.1 ms | <= 25.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | CONTROLLER_COMMAND period p99 after the flood (n=21, max 20.1 ms) | 20.1 ms |  | info |
| test_unknown_frame_flood_is_counted_and_not_disruptive | SENSOR_DATA accepted by the controller during the experiment | 99.9 frames/s | 95.0..105.0 | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | controller: no timeouts, no CRC or counter faults, no safe state, RUNNING |  | expected | ok |
| test_unknown_frame_flood_is_counted_and_not_disruptive | the flood stayed on its segment: the sensor saw no unknown frames, is RUNNING |  | expected | ok |
| test_flood_rate_sweep | 1000/s requested: achieved (1001 frames) | 999.9 frames/s |  | info |
| test_flood_rate_sweep | 1000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 1000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 1000/s: CONTROLLER_COMMAND period max during the flood (p99 20.13 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 1000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 2000/s requested: achieved (2001 frames) | 1999.9 frames/s |  | info |
| test_flood_rate_sweep | 2000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 2000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 5/32 | expected | ok |
| test_flood_rate_sweep | 2000/s: CONTROLLER_COMMAND period max during the flood (p99 20.08 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 2000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_flood_rate_sweep | 4000/s requested: achieved (4001 frames) | 3999.6 frames/s |  | info |
| test_flood_rate_sweep | 4000/s: counted as unknown | 100.0 % | >= 90.0 | ok |
| test_flood_rate_sweep | 4000/s: nothing dropped by the 32-frame receive queue | 0 dropped, high-water mark 7/32 | expected | ok |
| test_flood_rate_sweep | 4000/s: CONTROLLER_COMMAND period max during the flood (p99 20.12 ms) | 20.1 ms | <= 25.0 | ok |
| test_flood_rate_sweep | 4000/s: no timeouts or safe state, RUNNING |  | expected | ok |
| test_error_passive_entry_and_healing | ERROR_PASSIVE visible in the controller heartbeat after the injection (INJECT_CAN_ERROR x20: TEC 160) | 44.9 ms | 0..145.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (firmware log, ECU clock) (TEC heals by one per successful frame at 60/s: 33 frames, 550 ms expected) | 545.0 ms | 490.0..610.0 | ok |
| test_error_passive_entry_and_healing | time in error-passive (heartbeats, 100 ms granularity) | 599.6 ms |  | info |
| test_error_passive_entry_and_healing | CONTROLLER_COMMAND rate while error-passive (transmission continues) | 50.0 frames/s | 45.0..55.0 | ok |
| test_error_passive_entry_and_healing | one passive event, no bus-off, no safe state (error-passive is not an input loss) |  | expected | ok |
| test_error_passive_entry_and_healing | controller ERROR_ACTIVE and RUNNING again, TEC healed | state ERROR_ACTIVE tec=69 | expected | ok |
| test_bus_off_recovery_after_the_backoff | exactly one gap in the controller's commands | 1 | expected | ok |
| test_bus_off_recovery_after_the_backoff | CONTROLLER_COMMAND silence on the bus (bus-off, backoff, recovery) (backoff 100 ms + modelled recovery 3 ms + supervisor tick + next TX slot) | 139.9 ms | 100..168.0 | ok |
| test_bus_off_recovery_after_the_backoff | bus-off to error-active in the firmware log (ECU clock) | 105.0 ms | 100..118 | ok |
| test_bus_off_recovery_after_the_backoff | last_recovery_ms reported by `ecu status` | 105 ms |  | info |
| test_bus_off_recovery_after_the_backoff | one bus-off, one attempt, one recovery ok, no failure, phase active | phase active | expected | ok |
| test_bus_off_recovery_after_the_backoff | bus-off is an input loss: one safe-state entry, the first command after the gap is SAFE_STATE | first mode SAFE_STATE | expected | ok |
| test_bus_off_recovery_after_the_backoff | NORMAL again after the first command following recovery (recovery hold 500 ms) | 500.1 ms | 496..565.0 | ok |
| test_bus_off_recovery_after_the_backoff | heartbeat after recovery: BUS_OFF_SEEN flag set, bus_off_count 1 |  | expected | ok |
| test_bus_off_recovery_after_the_backoff | sensor: one CONTROLLER_COMMAND timeout, DEGRADED then RUNNING again | [00:00:02.640,482] <inf> ecu: node_state: RUNNING -> DEGRADED (critical input lost); [00:00:02.710,439] <inf> ecu: node_state: DEGRADED -> RUNNING (inputs nominal) | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #1: command silence (backoff 100 ms) | 119.8 ms | 100..168.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #2: command silence (backoff 200 ms (doubled: the previous recovery did not hold)) | 220.0 ms | 200..268.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #3: command silence (backoff 400 ms (doubled: the previous recovery did not hold)) | 420.1 ms | 400..468.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #4: command silence (backoff 800 ms (doubled: the previous recovery did not hold)) | 820.0 ms | 800..868.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #5: command silence (backoff 1600 ms (doubled: the previous recovery did not hold)) | 1620.0 ms | 1600..1668.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | every attempted recovery completed before the next bus-off (5) | [True, True, True, True, True] | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | bus-off #6: no recovery, phase failed, node FAULT, transmission stopped | phase failed, state FAULT, 0 commands after it | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | 5 attempts, 5 recoveries ok, 5 failures (the attempts are exhausted) | attempts 5, ok 5, failures 5 | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | first command after `ecu reset` (the reset restarts the backoff from the first attempt; the controller is still bus-off) | 112.9 ms | 100..188.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | NORMAL again after `ecu reset` (backoff 100 ms + recovery hold 500 ms) | 612.8 ms | 600..693.0 | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | controller RUNNING, CAN phase active after the reset | state RUNNING, phase active | expected | ok |
| test_repeated_bus_off_doubles_the_backoff_until_fault | sensor: one CONTROLLER_COMMAND timeout per silence (6), RUNNING again | 6 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller CRC rejections == frames the gateway corrupted | 22 rejected, 22 corrupted of 423 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 35 ms timeout) | 19 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_DATA: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 2 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | BATTERY_STATUS: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: runs of consecutive corrupted frames (longest 1; a run of rejects is silence to the 350 ms timeout) | 1 runs |  | info |
| test_corrupted_frames_are_rejected_by_crc | SENSOR_HEARTBEAT: timeouts == runs longer than the timeout (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller safe-state entries == timeout runs on critical inputs (0) | 0 | expected | ok |
| test_corrupted_frames_are_rejected_by_crc | controller RUNNING again; sensor unaffected |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | sensor corrupted 2 frames, controller rejected 2 by CRC | 2 corrupted, 2 bad on the bus, 2 rejected | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN clear in the heartbeat before the fault (RESET_FAULTS) |  | expected | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | CRC_FAULT_SEEN published in the controller heartbeat after the first bad frame (flag set on rejection, published by the next heartbeat) | 80.1 ms | 0..145.0 | ok |
| test_crc_fault_is_latched_in_the_heartbeat_flags | two bad frames are tolerated: no timeout, no safe state |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | sensor repeated the previous counter on 2 frames, controller rejected 2 as repeated | 2 frozen, 2 stale on the bus, 2 rejected | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN clear before the fault |  | expected | ok |
| test_stale_counter_is_rejected_as_repeated | COUNTER_FAULT_SEEN published after the first stale frame | 20.1 ms | 0..145.0 | ok |
| test_stale_counter_is_rejected_as_repeated | two stale frames are tolerated: no CRC fault, no timeout, no safe state | crc 0, timeouts 0, safe-state entries 0 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | controller repeated-counter rejections == frames the gateway duplicated | 41 rejected, 41 duplicated of 362 | expected | ok |
| test_stale_counter_is_rejected_as_repeated | duplicates cause no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | sensor skipped 3 counter values: controller accepted 3 frames with loss, 3 frames counted lost | 3 skipped, 3 accepted with loss, 3 lost | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | nothing rejected (the data is fresh), no timeout, no safe state |  | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | SENSOR_DATA frames dropped for 15 ms (largest gap on the bus 30.0 ms) | 2 frames |  | info |
| test_skipped_counter_is_accepted_and_the_loss_counted | timeouts == gaps over 35 ms (0) | 0 | expected | ok |
| test_skipped_counter_is_accepted_and_the_loss_counted | dropped == counted lost by the counter + frames inside timeout gaps | 2 dropped, 2 lost, 0 in timeout gaps | expected | ok |
| test_persistent_bad_frames_degrade_the_node | CRC rejections == corrupted BATTERY_STATUS frames (at least 3) | 3 rejected, 3 corrupted | expected | ok |
| test_persistent_bad_frames_degrade_the_node | DEGRADED in the heartbeat after the 3rd consecutive bad frame | 30.0 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | RUNNING again after the first good frame | 30.1 ms | 0..145.0 | ok |
| test_persistent_bad_frames_degrade_the_node | no safe state (battery is not a critical input), SENSOR_DATA untouched |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped by the gateway (controller counted 34 lost by counter and 1 timeouts; 1 gaps over 35 ms hold 3 frames) | 37 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: timeouts == gaps over the timeout | 1 vs 1 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_DATA: dropped == counted lost + frames inside timeout gaps | 37 vs 34 + 3 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped by the gateway (controller counted 1 lost by counter and 0 timeouts; 0 gaps over 350 ms hold 0 frames) | 1 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: timeouts == gaps over the timeout | 0 vs 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | SENSOR_HEARTBEAT: dropped == counted lost + frames inside timeout gaps | 1 vs 1 + 0 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped by the gateway (sensor counted 13 lost by counter and 1 timeouts; 1 gaps over 70 ms hold 3 frames) | 16 frames |  | info |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: timeouts == gaps over the timeout | 1 vs 1 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | CONTROLLER_COMMAND: dropped == counted lost + frames inside timeout gaps | 16 vs 13 + 3 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | controller safe-state entries == loss events on its critical inputs (1) | 1 | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | no CRC faults on either side (loss is not corruption); both RUNNING again |  | expected | ok |
| test_dropped_frames_are_accounted_by_the_counters | gateway: dropped sensor->controller / controller->sensor | 41 of 555 / 21 of 277 |  | info |
| test_duplicated_frames_are_rejected | controller repeated-counter rejections == frames the gateway duplicated | 34 rejected, 34 duplicated of 362 | expected | ok |
| test_duplicated_frames_are_rejected | COUNTER_FAULT_SEEN clear before, published in the controller heartbeat after |  | expected | ok |
| test_duplicated_frames_are_rejected | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p1 with 15 +/- 5 ms configured (n=285) | 10.4 ms | >= 9.500 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency median | 15.5 ms | 13.0..17.0 | ok |
| test_delayed_frames_are_not_losses | SENSOR_DATA latency p99 | 20.1 ms | <= 23.0 | ok |
| test_delayed_frames_are_not_losses | gateway max scheduled latency a->b / b->a | 19.98 / 19.98 ms |  | info |
| test_delayed_frames_are_not_losses | latency is not loss for a period-based timeout: no timeouts, no safe state, both RUNNING |  | expected | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: SENSOR_DATA latency median | 20.1 ms | 19.0..22.0 | ok |
| test_delayed_frames_are_not_losses | constant 20 ms delay: largest SENSOR_DATA gap on the controller's segment (the step into the delay) (period 10 ms + 20 ms step, below the 35 ms timeout) | 30.0 ms | <= 34 | ok |
| test_delayed_frames_are_not_losses | 20 ms of latency: no timeout, no counter fault, no safe state |  | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | SENSOR_DATA frames swapped with their successor (0 held frames released late without a successor) | 31 frames |  | info |
| test_reordered_frames_are_rejected_out_of_sequence | out-of-sequence rejections == swaps (the late frame is rejected) | 31 | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | accepted-with-loss events == swaps (the successor arrives with counter +2), one frame counted lost each | 31 accepted with loss, 31 lost | expected | ok |
| test_reordered_frames_are_rejected_out_of_sequence | no CRC faults, timeouts or safe state; both RUNNING |  | expected | ok |
| test_sensor_process_death_and_restart | SAFE_STATE command after the last SENSOR_DATA of the killed sensor (the 35 ms SENSOR_DATA timeout fires first; the heartbeat timeout would take 350 ms) | 59.4 ms | 31..100.0 | ok |
| test_sensor_process_death_and_restart | time from the kill to the SAFE_STATE command | 53.9 ms |  | info |
| test_sensor_process_death_and_restart | controller in SAFE_STATE while the sensor is dead | SAFE_STATE | expected | ok |
| test_sensor_process_death_and_restart | restarted sensor: first frame after the process start (process start, Zephyr boot, first TX tick) | 11.4 ms | <= 1000.0 | ok |
| test_sensor_process_death_and_restart | restarted sensor: first heartbeat after its first frame | 60.0 ms | 0..110.0 | ok |
| test_sensor_process_death_and_restart | NORMAL command after the first heartbeat of the restarted sensor (both critical inputs alive from the heartbeat on; hold 500 ms) | 512.1 ms | 496..565.0 | ok |
| test_sensor_process_death_and_restart | controller: one timeout per sensor message, one safe-state entry | {'SENSOR_DATA': 1, 'BATTERY_STATUS': 1, 'SENSOR_HEARTBEAT': 1} | expected | ok |
| test_sensor_process_death_and_restart | the restarted sensor's counters (from 0) are accepted: no counter rejections, no CRC faults (the timeout reset the receiver's counter history) |  | expected | ok |
| test_sensor_process_death_and_restart | both RUNNING at the end |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | exactly one gap in SENSOR_DATA | 1 | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | SENSOR_DATA silence on the bus (stall, watchdog reset, reboot, first TX tick) | 1322.5 ms |  | info |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog reset after the last frame (firmware uptime dates the reboot) (the supervisor feeds the watchdog until the stalled thread is 300 ms stale, then the watchdog's 1000 ms run out) | 1312.4 ms | 1290..1330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | reboot to the first frame of the rebooted sensor (host: exec, Zephyr boot, first TX tick) (host time, not firmware time: the executable is re-executed from the bind mount) | 10.2 ms | <= 1500.0 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | watchdog feed withheld after the liveness window (firmware log, ECU clock) (liveness window 300 ms) | 305.1 ms | 290..330 | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor rebooted in place: boot banner again, uptime restarted, same process | uptime 1694 ms, same pid | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | controller: one safe-state entry, RUNNING again |  | expected | ok |
| test_stalled_thread_is_reset_by_the_watchdog | sensor RUNNING again |  | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | heartbeats suppressed by the sensor (SUPPRESS_HEARTBEAT 600 ms) | 6 frames |  | info |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SENSOR_HEARTBEAT silence on the bus (600 ms of suppression starts and ends between two 100 ms heartbeat slots) | 700.0 ms | 590..810.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | SAFE_STATE command after the last heartbeat (heartbeat timeout 350 ms + supervisor tick + command period) | 369.6 ms | 346..415.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | NORMAL command after the first heartbeat again (recovery hold 500 ms + tick + command period) | 509.6 ms | 496..565.0 | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | every command in the safe state: actuators off, brake hold, speed limit 0, torque 0 | 42 commands over 840 ms | expected | ok |
| test_heartbeat_loss_drives_the_controller_into_safe_state | controller: one safe-state entry, one SENSOR_HEARTBEAT timeout, SENSOR_DATA never timed out, RUNNING again |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: frames dropped by the gateway (435 ms of loss, period 10 ms) | 44 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 35 ms + supervisor tick 5 ms + 20 ms publish period) | 49.3 ms | 31..100.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 519.2 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_DATA, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_DATA] | SENSOR_DATA: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: frames dropped by the gateway (750 ms of loss, period 100 ms) | 8 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller reaction (CONTROLLER_COMMAND.mode=SAFE_STATE) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 20 ms publish period) | 359.3 ms | 346..415.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller back to NORMAL after the first frame delivered again (recovery hold 500 ms + tick + publish period) | 519.3 ms | 496..565.0 | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: exactly one timeout counted by the controller, none for its other inputs | 1 for SENSOR_HEARTBEAT, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: controller safe-state entries == 1, RUNNING again | 1 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[SENSOR_HEARTBEAT] | SENSOR_HEARTBEAT: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: frames dropped by the gateway (750 ms of loss, period 100 ms) | 8 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller reaction (CONTROLLER_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 350 ms + supervisor tick 5 ms + 100 ms publish period) | 430.3 ms | 346..495.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 29.6 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: exactly one timeout counted by the controller, none for its other inputs | 1 for BATTERY_STATUS, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: controller safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[BATTERY_STATUS] | BATTERY_STATUS: sensor unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: frames dropped by the gateway (470 ms of loss, period 20 ms) | 24 frames |  | info |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor reaction (SENSOR_HEARTBEAT.node_state=DEGRADED) after the last delivered frame (timeout 70 ms + supervisor tick 5 ms + 100 ms publish period) | 140.6 ms | 66..215.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor back to RUNNING after the first frame delivered again (no hold before leaving DEGRADED) | 40.6 ms | 0..145.0 | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: exactly one timeout counted by the sensor, none for its other inputs | 1 for CONTROLLER_COMMAND, 0 others | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: sensor safe-state entries == 0, RUNNING again | 0 entries, state RUNNING | expected | ok |
| test_missing_message_is_detected_within_its_timeout[CONTROLLER_COMMAND] | CONTROLLER_COMMAND: controller unaffected (no timeouts, no safe state, RUNNING) |  | expected | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period mean (n=499, nominal 10 ms) | 10.0 ms | 9.900..10.1 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period std | 0.021 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period p99 | 10.0 ms | 9.000..11.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period min | 9.733 ms | >= 7.500 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA period max | 10.1 ms | <= 12.5 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_DATA rate | 100.0 frames/s | 98.0..102.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period std | 0.016 ms |  | info |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period p99 | 100.0 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period min | 100.0 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS period max | 100.0 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | BATTERY_STATUS rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period std | 0.010 ms |  | info |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period p99 | 100.0 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period min | 100.0 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT period max | 100.0 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | SENSOR_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period mean (n=249, nominal 20 ms) | 20.0 ms | 19.8..20.2 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period std | 0.052 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period p99 | 20.1 ms | 18.0..22.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period min | 19.6 ms | >= 15.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND period max | 20.3 ms | <= 25.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_COMMAND rate | 50.0 frames/s | 49.0..51.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period mean (n=49, nominal 100 ms) | 100.0 ms | 99.0..101.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period std | 0.344 ms |  | info |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period p99 | 100.2 ms | 90.0..110.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period min | 98.9 ms | >= 75.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT period max | 100.2 ms | <= 125.0 | ok |
| test_periodic_messages_stay_within_tolerance | CONTROLLER_HEARTBEAT rate | 10.0 frames/s | 9.800..10.2 | ok |
| test_periodic_messages_stay_within_tolerance | sensor: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | sensor: watchdog fed on every supervisor tick | feeds 1103, skipped 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: no TX tick overruns, nothing busy or rejected | overruns 0, busy 0, rejected 0 | expected | ok |
| test_periodic_messages_stay_within_tolerance | controller: watchdog fed on every supervisor tick | feeds 1106, skipped 0 | expected | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2900.0 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | SENSOR_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.056 ms | <= 15.0 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: firmware ms per host ms (2900 ms of uptime over 2899.7 ms of bus time, 30 heartbeats) | 1.000 | 0.995..1.005 | ok |
| test_firmware_clock_tracks_bus_time | CONTROLLER_HEARTBEAT: largest firmware-clock deviation from bus time (heartbeat uptime is stamped at assembly, up to one TX slot before the frame) | 0.923 ms | <= 15.0 | ok |

## 4. Earlier phases

The numbers from Phases 2 and 3 quoted in the README are recorded with their
full console output in the phase logs:

| Number | Command | Raw data |
|---|---|---|
| Safe state 54.1 ms after the last sensor frame; back to normal 525 ms after the restarted sensor's first heartbeat | `tools/dev.sh python3 tools/ecu_network.py safe-state` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Simulated bus-off recovered in 103 ms, one 120 ms command gap | `tools/dev.sh python3 tools/ecu_network.py bus-off` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Relay latency median 0.13 ms, p99 0.27 ms; safe state 374.7 ms after a lost heartbeat; 50 of 50 dropped frames accounted for; 1313 ms of silence before the watchdog reset | `tools/dev.sh python3 tools/fault_scenarios.py --seed 1` | [docs/phase-logs/phase3.md](docs/phase-logs/phase3.md), [DESIGN.md §12.5](DESIGN.md#125-measured-results-simulated-bus-on-a-desktop-os) |

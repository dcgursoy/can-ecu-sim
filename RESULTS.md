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

## 2. Earlier phases

The numbers from Phases 2 and 3 quoted in the README are recorded with their
full console output in the phase logs:

| Number | Command | Raw data |
|---|---|---|
| Safe state 54.1 ms after the last sensor frame; back to normal 525 ms after the restarted sensor's first heartbeat | `tools/dev.sh python3 tools/ecu_network.py safe-state` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Simulated bus-off recovered in 103 ms, one 120 ms command gap | `tools/dev.sh python3 tools/ecu_network.py bus-off` | [docs/phase-logs/phase2.md](docs/phase-logs/phase2.md) |
| Relay latency median 0.13 ms, p99 0.27 ms; safe state 374.7 ms after a lost heartbeat; 50 of 50 dropped frames accounted for; 1313 ms of silence before the watchdog reset | `tools/dev.sh python3 tools/fault_scenarios.py --seed 1` | [docs/phase-logs/phase3.md](docs/phase-logs/phase3.md), [DESIGN.md §12.5](DESIGN.md#125-measured-results-simulated-bus-on-a-desktop-os) |

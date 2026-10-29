# Phase 4 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2,
image `can-ecu-sim-dev:local` with python-can 4.6.1, cantools 40.7.1 and Python
3.12.3). Every command ran through `tools/dev.sh` inside the dev image. Per the
review policy, this phase (Python tooling and documentation) had no adversarial
review. All timing numbers are kernel timestamps of a simulated bus on a desktop
OS, not real-time hardware. The repository was pushed to GitHub at the start of
the phase (`https://github.com/dcgursoy/can-ecu-sim`), so CI ran for the first
time; section 6 covers what it found.

## 1. Unit tests

```
$ python3 -m pytest -q host/ can/ gateway/
.............................................                            [100%]
45 passed in 2.25s
```

Twenty of them are the new `host/` tests, which need no `vcan`: the analysis on
synthetic frames with known answers (the CAN frame bit counts 47/111/135, the
percentile interpolation, the bus load and the DBC nominal of 3.996 % at
500 kbit/s, period statistics with a planted 40 ms gap, event-driven counts,
unknown identifiers, the nine protection verdicts of the receiver policy, echo
matching across a `seq` wrap, a missed probe and a stray response, single-node
targets, the text and JSON reports, an empty capture), the decoder (values,
period and verdict, unknown and short frames, the stream line and the table,
the command line on a three-line log with and without the direction marker,
identifier parsing), the logger (the recorder on python-can's in-process virtual
interface with the CSV columns, values and verdicts checked row by row, and the
command line with a duration) and the echo probe against a fake responder on
the virtual interface. The other 25 are the Phase 1 reference tests and the
Phase 3 fault-engine tests, unchanged. After the fixes of section 5 the host
tests were run again: `20 passed in 1.99s`.

## 2. Run 1 (seed 1): the first live run

`python3 tools/record_and_analyze.py --retries 1`, before the direction-marker
fix of section 5 (which changes a letter in `bus.log`, not a number) and before
the analysis learned to comment on an all-zero turnaround. Result OK. The
analysis of the recording:

```
$ python3 tools/record_and_analyze.py --retries 1
=== python3 -m host.analysis bus.log --bitrate 500000 (the recording of python3 -m host.logger)
capture: bus.log: 2038 frames over 7.978 s on vcan0; unknown identifiers: none
protection: 1438 protected frames, dlc faults 0, crc faults 0, repeated counters 0, out-of-sequence 0, frames lost per the counters 0
every timing number below: measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware

bus load at 500 kbit/s (CAN 2.0A, 11-bit identifiers: what this traffic would occupy on a real bus; vcan has no bit rate)
  message                     id  frames  rate/s dlc    bit/s   share  worst-case bit/s
  CONTROLLER_COMMAND       0x080     400    50.1   8     5565   20.2 %              6768
  SENSOR_DATA              0x100     798   100.0   8    11102   40.3 %             13503
  BATTERY_STATUS           0x110      80    10.0   8     1113    4.0 %              1354
  SENSOR_HEARTBEAT         0x200      80    10.0   8     1113    4.0 %              1354
  CONTROLLER_HEARTBEAT     0x201      80    10.0   8     1113    4.0 %              1354
  ECHO_REQUEST             0x300     200    25.1   6     2381    8.6 %              2883
  SENSOR_ECHO_RESPONSE     0x301     200    25.1   7     2582    9.4 %              3133
  CONTROLLER_ECHO_RESPONSE 0x302     200    25.1   7     2582    9.4 %              3133
  total 27551 bit/s = 5.51 % of 500 kbit/s (worst-case stuffing 6.70 %)
  periodic messages 4.00 % measured vs 4.00 % nominal from the DBC cycle times (4.86 % worst case); busiest 100 ms window 7.67 %

period and jitter in ms (consecutive frames of one identifier; gaps: periods over 3 x nominal, a process not scheduled)
  message                  nominal      n  rate/s     mean    std     min      max     p50     p99 peak dev  gaps
  CONTROLLER_COMMAND          20.0    400    50.0   19.996  0.472   18.28    21.35   19.99   21.26     1.72     0
  SENSOR_DATA                 10.0    798   100.0    9.997  0.428    8.38    11.43   10.00   11.17     1.62     0
  BATTERY_STATUS             100.0     80    10.0   99.983  0.331   98.11   100.44  100.08  100.38     1.89     0
  SENSOR_HEARTBEAT           100.0     80    10.0   99.989  0.590   97.91   101.30  100.08  101.30     2.09     0
  CONTROLLER_HEARTBEAT       100.0     80    10.0   99.983  0.557   98.00   101.29  100.07  101.26     2.00     0
  event-driven: CONTROLLER_ECHO_RESPONSE x200, ECHO_REQUEST x200, SENSOR_ECHO_RESPONSE x200

responder         sent  answ  unm   rtt min     p50     p90     p99     max   turnaround p50     p99   rtt-turnaround p50     p99
CONTROLLER_ECU     200   200    0     0.086   0.723   1.643   2.166   2.248            0.000   0.000                0.723   2.166
SENSOR_ECU         200   200    0     0.117   0.710   1.726   2.220   4.821            0.000   0.000                0.710   2.220
all in ms. rtt: ECHO_REQUEST on the bus to the response on the bus (kernel timestamps); turnaround: the ECU's own receive-to-transmit time from its response; the difference is the kernel, the CAN driver and the OS scheduling of the ECU process
```

The ECUs' `ecu status` after the run: both RUNNING, `safe_state_entries=0`,
`dropped=0`, `wdt skipped=0`, `echo_responses=200` on each, `queue_hwm=3/32`,
and `unknown=200` on each because each ECU sees the other's echo responses,
which are not in its receive set.

## 3. Run 2 (seed 2): the run quoted in README and RESULTS.md

`python3 tools/record_and_analyze.py --retries 1 --seed 2` on the final code,
complete console output:

```
$ python3 tools/record_and_analyze.py --retries 1 --seed 2
=== python3 -m host.analysis bus.log --bitrate 500000 (the recording of python3 -m host.logger)
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
=== host.echo as matched live by the probe itself (200 probes to ALL, 20 ms apart; same kernel timestamps)
responder         sent  answ  unm   rtt min     p50     p90     p99     max   turnaround p50     p99   rtt-turnaround p50     p99
CONTROLLER_ECU     200   200    0     0.044   0.582   1.029   1.195   1.204            0.000   0.000                0.582   1.195
SENSOR_ECU         200   200    0     0.031   0.544   1.018   1.155   1.187            0.000   0.000                0.544   1.155
all in ms. rtt: ECHO_REQUEST on the bus to the response on the bus (kernel timestamps); turnaround: the ECU's own receive-to-transmit time from its response; the difference is the kernel, the CAN driver and the OS scheduling of the ECU process
turnaround is 0 in every response: the ECU's code ran in zero simulated time, as it does on Zephyr's native_sim, so the whole round trip is host time (the field is informative on hardware)
(measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware)
=== python3 -m host.decoder --mode table, final table (exit 0)
message                     id   count  rate/s  period ms mean/last/max  dlc  crc  rep  oos  lost  last values
CONTROLLER_COMMAND       0x080     394    50.0    20.00 /  20.03 /  20.33    0    0    0    0     0  mode=NORMAL torque_cmd=-50.00 Nm steering_cmd=-3.10 deg actuators_enabled=1 brake_hold=0 fault_latched=0 speed_limit=25.0 m/s
SENSOR_DATA              0x100     787   100.0    10.00 /  10.05 /  10.53    0    0    0    0     0  sensor_status=OK speed=17.87 m/s yaw_rate=6.2 deg/s accel_x=0.57 m/s^2
BATTERY_STATUS           0x110      79    10.0   100.00 / 100.04 / 100.33    0    0    0    0     0  charging=0 low_voltage=0 over_temperature=0 pack_voltage=47.86 V pack_current=31.8 A state_of_charge=90.0 % temperature=32 degC
SENSOR_HEARTBEAT         0x200      79    10.0   100.01 / 100.09 / 100.36    0    0    0    0     0  node_state=RUNNING uptime=7870 ms can_state=ERROR_ACTIVE bus_off_count=0 error_flags=0
CONTROLLER_HEARTBEAT     0x201      78    10.0   100.00 / 100.11 / 100.34    0    0    0    0     0  node_state=RUNNING uptime=7770 ms can_state=ERROR_ACTIVE bus_off_count=0 error_flags=0
ECHO_REQUEST             0x300     200     0.0    20.00 /  20.00 /  20.45    0    0    0    0     0  target_node=ALL seq=199 payload=4106092743
SENSOR_ECHO_RESPONSE     0x301     200     0.0    20.00 /  19.82 /  21.11    0    0    0    0     0  seq=199 payload=4106092743 turnaround=0 us
CONTROLLER_ECHO_RESPONSE 0x302     200     0.0    20.00 /  19.81 /  21.01    0    0    0    0     0  seq=199 payload=4106092743 turnaround=0 us
rate: frames per second over the last second of bus time; rep/oos: repeated / out-of-sequence counters; lost: frames the counters say were missed
(measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware)
=== python3 -m host.logger summary (exit 0)
captured 2040 frames over 7.991 s on vcan0: /tmp/can-ecu-sim-logs/bus.log, /tmp/can-ecu-sim-logs/bus.csv
  CONTROLLER_COMMAND     0x080     400 frames    50.0/s (nominal 50/s)
  SENSOR_DATA            0x100     800 frames   100.0/s (nominal 100/s)
  BATTERY_STATUS         0x110      80 frames    10.0/s (nominal 10/s)
  SENSOR_HEARTBEAT       0x200      80 frames    10.0/s (nominal 10/s)
  CONTROLLER_HEARTBEAT   0x201      80 frames    10.0/s (nominal 10/s)
  ECHO_REQUEST           0x300     200 frames    50.0/s (event-driven)
  SENSOR_ECHO_RESPONSE   0x301     200 frames    50.0/s (event-driven)
  CONTROLLER_ECHO_RESPONSE 0x302     200 frames    50.0/s (event-driven)
(measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware)
recording time 8.3 s
--- sensor `ecu status`
  ecu: role=sensor test_hooks=1 uptime_ms=7990
  node: state=RUNNING reason=inputs established since_ms=7910 safe_state_entries=0 error_flags=0x00
  can: phase=active state=ERROR_ACTIVE tec=0 rec=0 manual_recovery=0 simulated=0 bus_off=0 passive=0 recovery_attempts=0 recoveries_ok=0 recovery_failures=0 last_recovery_ms=-1 backoff_ms=100
  tx: sent=1159 ok=1158 err=0 busy=0 rejected=0 blocked_bus_off=0 tick_overruns=0
  rx_summary: frames=879 unknown=200 queue_hwm=2/32 dropped=0 events_dropped=0 echo_responses=200 test_commands=0
  rx CONTROLLER_COMMAND: alive=1 age_ms=12 ok=399 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  rx CONTROLLER_HEARTBEAT: alive=1 age_ms=11 ok=80 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  wdt: available=1 timeout_ms=1000 feeds=1598 skipped=0 stale_threads=0x0
  hooks: suppressed=0 corrupted=0 skipped=0 frozen=0
--- controller `ecu status`
  ecu: role=controller test_hooks=1 uptime_ms=7990
  node: state=RUNNING reason=inputs established since_ms=7925 safe_state_entries=0 error_flags=0x00
  can: phase=active state=ERROR_ACTIVE tec=0 rec=0 manual_recovery=0 simulated=0 bus_off=0 passive=0 recovery_attempts=0 recoveries_ok=0 recovery_failures=0 last_recovery_ms=-1 backoff_ms=100
  tx: sent=680 ok=679 err=0 busy=0 rejected=0 blocked_bus_off=0 tick_overruns=0
  rx_summary: frames=1359 unknown=200 queue_hwm=2/32 dropped=0 events_dropped=0 echo_responses=200 test_commands=0
  rx SENSOR_DATA: alive=1 age_ms=8 ok=799 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  rx SENSOR_HEARTBEAT: alive=1 age_ms=28 ok=80 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  rx BATTERY_STATUS: alive=1 age_ms=57 ok=80 crc=0 counter=0 dlc=0 skipped=0 lost=0 timeouts=0
  wdt: available=1 timeout_ms=1000 feeds=1598 skipped=0 stale_threads=0x0
  hooks: suppressed=0 corrupted=0 skipped=0 frozen=0
(every number above: measured on a simulated bus (Linux vcan) on a desktop OS, not on real-time hardware)
RESULT: OK
logs copied to /workspaces/can-ecu-sim/build/network-logs/record-and-analyze
```

The two runs agree on every mean and on the bus load and differ in the spread:
SENSOR_DATA period standard deviation 0.43 ms in run 1 against 0.10 ms in
run 2, worst period 11.43 against 10.53 ms, echo round trip p99 2.2 against
1.2 ms and a single 4.8 ms maximum in run 1. Run 1 started while the
containers of the previous commands were still exiting; the spread is the host.

## 4. Cross-checks of the recording

The run-1 recording, written by python-can's log writer with its ` T` direction
marker, verified by the Phase 1 verifier on its widened reader:

```
$ python3 can/verify_candump.py --min-protected 1000 build/network-logs/record-and-analyze/bus.log
verified 2038 frames (1438 protected, CRC-checked): 0 wrong DLC, 0 bad CRC, 0 counter faults, 0 unknown ids, 0 unparsed lines -> OK
```

The analysis command line on the published file reproduces the report
(`python3 -m host.analysis build/network-logs/record-and-analyze/bus.log
--json /tmp/a.json`: 2038 frames over 7.978 s, echo p50 0.723 / 0.710 ms), the
probe's own live matching printed the same percentiles as the analysis of the
recording in both runs, and the ECUs' `echo_responses=200` equals the 200
responses matched for each. The Phase 2 demo on the widened reader:

```
$ python3 tools/ecu_network.py --retries 1 run --duration 3
RESULT: OK
```

## 5. What python-can does on vcan

Two findings from reading the first recording rather than from the unit tests,
which pass on the virtual interface where neither occurs:

- Every line of the first `bus.log` ended in ` T`. python-can derives
  `Message.is_rx` from the kernel's `MSG_DONTROUTE` flag ("set when the
  received frame was created on the local host"), and on a `vcan` interface
  every frame is created on the local host, so python-can marks all of them as
  transmitted. The logger now sets `is_rx` itself (it only receives), the probe
  marks its own requests by identifier, and `host.capture.Frame` documents that
  the flag is not to be relied on.
- python-can writes the newer candump log format with the direction marker at
  the end of the line (`(ts) vcan0 100#... R`), which the project's readers in
  `tools/ecu_network.py` and `can/verify_candump.py` rejected. Their regular
  expressions accept an optional marker now; python-can's own reader always
  did.

## 6. The first CI run and the allocator check

The push of the Phase 3 commit started the first GitHub Actions run
([40795735230](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40795735230)).
Every step passed, image build and push to GHCR included, through the four
firmware builds; then `No dynamic allocation after init (all four images)`
failed on the first image with

```
== final image zephyr/zephyr.exe
ERROR: zephyr/zephyr.exe has no main/z_cstart symbol; not a Zephyr image?
```

and the six vcan steps after it were skipped. The same check passes locally,
also as root like CI. The run's `firmware-builds` artifact holds the very
`zephyr.exe` that failed; `nm` on it inside the dev image:

```
08049dae t main
080583c3 T main
08055d2d t z_cstart
```

56 KB of symbols, `main` at byte 35924. The check's only non-deterministic
element was `echo "$SYMS" | grep -q` under `set -o pipefail`: `grep -q` exits
at the first match, the subshell still writing the remaining 20 KB in 4 KB
chunks gets SIGPIPE, and `pipefail` turns the pipeline's status 141 into a
failed test. The retry loop printed nothing because its own `grep -q` had
matched; the final check raced again and lost. Reproduced on the CI image in the
container with eight busy loops running: 26 failures in 2000 tries with the old
pattern, 0 in 2000 with `grep -c` (which reads all input); idle, 0 in 200 with
either, which is why it looked like a property of the bind mount in Phase 3.
`tools/check-no-heap.sh` now reads the symbol table once and consumes it fully
in every test; all four local images and the CI-built image pass 200 of 200
tries. The Phase 3 log carries a correction.

## 7. What the simulator taught

Every echo response carries `turnaround = 0 µs`. On `native_sim`,
`k_cycle_get_32()` returns the simulated clock (`drivers/timer/native_sim_timer.c`:
`sys_clock_cycle_get_32()` is `nsi_hws_get_time()`), and firmware code runs in
zero simulated time, so the RX thread's work between dequeuing the request and
calling transmit takes no time by the ECU's own clock. The round trip is the
simulator's CAN driver and the host: `drivers/can/can_native_linux.c` polls the
socket with a zero-timeout `select()` and then `k_sleep(K_MSEC(1))` ("short
sleep required to avoid blocking the whole native process"), so a request waits
0–1 ms for the next poll; the medians of 0.54–0.72 ms and the 99th percentiles
of 1.2–2.2 ms are that interval plus scheduling. The analysis prints a note when
it finds every turnaround zero. On the Nucleo the field would come from the
cycle counter and the round trip would include two real frame times.

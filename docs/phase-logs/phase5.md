# Phase 5 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2,
image `can-ecu-sim-dev:local`, pytest 8, GoogleTest, gcovr 8). Every command
ran through `tools/dev.sh` inside the dev image. Per the review policy, this
phase (a Python test suite and host unit tests, everything testable locally)
had no adversarial review. All timing numbers are kernel timestamps of a
simulated bus on a desktop OS, not real-time hardware; the firmware's own
constants are the Kconfig defaults read from the controller build's `.config`.

## 1. The suite

`tests/`: `harness.py` (the network under test, experiments, host-stall
policy, the measurement log), `conftest.py` (options, fixtures, the printing
and the `measurements.json` / `measurements.md` writers), eight modules with
23 tests for the seven groups (the bus-off group is two modules, section 2 says why).
`pytest.ini` makes a bare `pytest` run the unit suites only (`testpaths = can
gateway host`); the system suite is explicit: `python3 -m pytest tests -v`.

## 2. First full run

`python3 -m pytest tests -v` on the normal builds, 90 s: 20 of 23 passed on
the first run, three failed, all three for reasons in the tests rather than in
the firmware.

All three in the test design, none in the firmware:

- `test_repeated_bus_off_doubles_the_backoff_until_fault` measured silences of
  220, 420, 840 and 1640 ms where it expected 100, 200, 400 and 800: its first
  bus-off came 1.5 s after the previous test's recovery, inside the CAN error
  state machine's 10 s stable window, so the firmware counted it as "a recovery
  that did not hold" and started at the doubled backoff. That is the designed
  behaviour (`ecu/can_error_sm.hpp`, verified by the unit test
  `RecoveryThatDoesNotHoldCountsAsAFailedAttempt`), and `RESET_FAULTS` clears
  the attempt count but not the time of the last recovery. The chain now has a
  module of its own, which gives it a fresh controller process, polls the CAN
  phase before every further bus-off (a bus-off during the backoff is the same
  event and would be swallowed), and a retry waits the stable window out.
- `test_crc_fault_is_latched_in_the_heartbeat_flags` found CRC_FAULT_SEEN
  already set "before the fault": its look-back window of one second reached
  into the previous test, which had injected CRC faults. The window now starts
  at the snapshot taken after `RESET_FAULTS`.
- `test_stale_counter_is_rejected_as_repeated` froze the counter on three
  frames and got one SENSOR_DATA timeout and a safe-state entry on top of the
  three rejections: a rejected frame does not renew the timeout, so three
  consecutive rejects of a 10 ms message leave 40 ms between accepted frames,
  past the 35 ms limit. Phase 3's scenario used two for exactly this reason;
  the test does too and says why, and the five-frame case of the corrupt-CRC
  test shows the other side.

## 3. Final run with firmware coverage

`tools/run-system-tests.sh --coverage` (the two instrumented builds, the
suite, gcovr): 23 of 23 passed in 94 s on 2026-10-29, seed 1, no attempt inconclusive
(the run quoted in the README and in DESIGN.md section 14; its complete
measured-value table is `RESULTS.md` section 2.1). The headline numbers:

| What | Measured | Limit |
|---|---|---|
| Periodic timing, SENSOR_DATA (10 ms) | mean 10.001 ms, std 0.109 ms, p99 10.3 ms, min 9.4 ms, max 10.5 ms | mean within 1 %, p99 within 10 %, every period within 25 % |
| Periodic timing, CONTROLLER_COMMAND (20 ms) | mean 20.002 ms, std 0.104 ms, p99 20.3 ms, max 20.5 ms | same |
| Firmware clock against bus time | 1.000 firmware ms per host ms, largest deviation 0.84 ms over 3 s | 0.995..1.005 |
| Detection of a missing message: SENSOR_DATA lost (35 ms timeout) | reaction on the bus 51.3 ms after the last delivered frame; back to normal 521.4 ms after the first frame again | 31..100.0 / 496..565.0 ms |
| Detection of a missing message: SENSOR_HEARTBEAT lost (350 ms timeout) | reaction on the bus 371.0 ms after the last delivered frame; back to normal 511.2 ms after the first frame again | 346..415.0 / 496..565.0 ms |
| Detection of a missing message: BATTERY_STATUS lost (350 ms timeout, DEGRADED) | reaction on the bus 421.5 ms after the last delivered frame; back to normal 20.9 ms after the first frame again | 346..495.0 / 0..145.0 ms |
| Detection of a missing message: CONTROLLER_COMMAND lost at the sensor (70 ms timeout, DEGRADED) | reaction on the bus 168.7 ms after the last delivered frame; back to normal 69.0 ms after the first frame again | 66..215.0 / 0..145.0 ms |
| Corrupted frames (gateway, 10 % of sensor frames) | 22 rejected, 22 corrupted of 422; 0 timeouts, 0 safe-state entries | exact counts |
| CRC fault published in the heartbeat flags | 61.9 ms after the first bad frame | 0..145.0 ms |
| Stale counter (frozen x2) and duplicates (20 %) | 2 frozen, 2 stale on the bus, 2 rejected; 41 rejected, 41 duplicated of 362; flag after 72.1 ms | exact counts |
| Skipped counter (x3) and a 15 ms loss | 3 skipped, 3 accepted with loss, 3 lost; 2 dropped, 2 lost, 0 in timeout gaps | exact counts |
| Three consecutive bad BATTERY_STATUS frames | DEGRADED 20.9 ms after the third, RUNNING 20.9 ms after the first good frame | 0..145 ms |
| Flood, 2000 unknown frames/s for 2 s on the controller's segment | 1999.9 frames/s achieved, 100.0 % counted, 0 dropped, queue high-water mark 5/32; CONTROLLER_COMMAND period p99 20.3 ms, max 20.7 ms; SENSOR_DATA still accepted at 99.9 /s | no drops, p99 within 10 % |
| Flood sweep 1000 / 2000 / 4000 frames/s | achieved 999.9 / 1999.6 / 3999.4 frames/s, 100 % counted, nothing dropped, command period max 20.4 / 20.2 / 20.2 ms | no drops, max within 25 % |
| Error-passive (INJECT_CAN_ERROR x20, TEC 160) | visible in the heartbeat after 31.0 ms; 530.0 ms in error-passive (550 expected from TEC arithmetic); commands kept flowing at 50.0 /s | 490.0..610.0 ms |
| Bus-off and recovery (FORCE_BUS_OFF) | command silence 140.0 ms; bus-off to error-active 105.0 ms in the firmware log; SAFE_STATE entered, NORMAL again 499.9 ms after the first command | 100..168.0 / 496..565.0 ms |
| Repeated bus-off: doubling backoff, FAULT, `ecu reset` | silences 140.0 / 220.0 / 420.0 / 820.0 / 1620.2 ms, FAULT on the sixth with transmission stopped; after `ecu reset` first command 127.4 ms, NORMAL 627.4 ms | 100 / 200 / 400 / 800 / 1600 ms + 68; reset 100..188 / 600..693 ms |
| Heartbeat loss (SUPPRESS_HEARTBEAT 600 ms) | SAFE_STATE 374.8 ms after the last heartbeat, NORMAL 513.9 ms after the first again; 42 commands over 840 ms with actuators off, brake hold, speed limit 0 | 346..415.0 / 496..565.0 ms |
| Sensor process killed and restarted | SAFE_STATE 50.0 ms after the last SENSOR_DATA; restarted sensor's first frame 19.2 ms after the process start; NORMAL 507.5 ms after its first heartbeat; counters from zero accepted | 31..100.0 / 496..565.0 ms |
| Stalled TX thread (STALL_THREAD 1500 ms) | feed withheld 295.0 ms after the stall, watchdog reset 1301.6 ms after the last frame, first frame 11.6 ms after the reboot (1313.2 ms of silence in all); same process, controller safe state once | 290..330 / 1290..1330 ms |
| Random loss, 10 % both ways for 3 s | 36 vs 36 + 0 (SENSOR_DATA); 4 vs 4 + 0 (SENSOR_HEARTBEAT); 16 vs 13 + 3 (CONTROLLER_COMMAND); dropped = lost by counter + frames inside timeout gaps | exact accounting |
| Duplicates, 20 % for 2 s | 34 rejected, 34 duplicated of 362 | exact |
| Delay 15 +/- 5 ms, then a constant 20 ms | SENSOR_DATA latency p1 10.4 ms, median 15.6 ms, p99 20.2 ms, no timeout; 20 ms step: largest gap 30.2 ms, no timeout | median 13..17, p99 <= 23; gap <= 34 ms |
| Reordering, 20 % of SENSOR_DATA for 2 s | 31 swaps: 31 out-of-sequence rejections, 31 accepted with loss, 31 lost | exact |

Firmware line coverage under the suite, from the application-instrumented
builds: 90.5 % of lines (939 of 1037),
92.7 % of functions, 57.6 % of branches
(`RESULTS.md` section 2.2 has the per-file report). The first attempt at this
number used `CONFIG_COVERAGE`, which instruments the kernel too: 125 data files
per process exit, 2.5 s on the bind mount, and the harness's two-second grace
after SIGTERM lost the application's files at random, 88.1 % in one run and
85.5 % in the next. With only the seven application objects instrumented a
process exits in 80 ms and the number is reproducible.

## 4. Unit tests and coverage

`tools/run-host-tests.sh`: 77 GoogleTest cases (70 before this phase),
100.0 % of lines (937 of 937),
100.0 % of functions, 85.4 % of branches of the
library headers (95.1 / 94.5 / 81.5 % before). The seven new cases: a bus-off
reported through the callback and the poll while already being handled, a tick
while error-passive and before the stable window with `reset()` outside FAULT,
a bus-off policy from run-time values, a node policy from run-time values with
`reset()` outside FAULT, a liveness window from a run-time value, a timeout
configuration from run-time values, and an out-of-table value for every enum.
Two of the lines the first report showed as missing were not missing tests
but the way gcov sees `constexpr`: a constructor called with constant
arguments is evaluated by the compiler and never runs, and a member template
instantiated only inside a `static_assert` is emitted (with
`-fkeep-inline-functions`) but never executed, and gcov reports its lines
uncovered although other instantiations of the same lines ran. The
compile-time scenarios are now also called at run time, which doubles as a
check that both evaluations agree.

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

## 5. CI

`.github/workflows/ci.yml` runs `tools/dev.sh tools/run-system-tests.sh
--coverage` after the host-tools step and uploads `build/test-results/` as the
`system-tests` artifact (JUnit report with every measured value as a test
property, `measurements.json` and `.md`, the logs of every module, the firmware
coverage report). The unit-test step is unchanged (`pytest -q can/ gateway/
host/`; a bare `pytest` now runs the same three suites through `pytest.ini`).
The outcome of the CI run for this commit is recorded in the Phase 6 log.

## 6. What Phase 5 taught

- **Test history is state.** The firmware remembers things across tests that
  a test cannot see from the outside: the time of the last bus-off recovery
  decides whether the next bus-off is a fresh event or a failed attempt. A
  suite that shares one process between tests inherits that history; a fresh
  process per module costs two seconds of boot and buys independence.
- **Every limit should name its mechanism.** Writing each limit as timeout +
  supervisor tick + publishing period + host allowance made the three
  first-run failures diagnosable from their numbers alone: 220 ms against a
  100 ms backoff is a doubling, not jitter.
- **Coverage tools count what ran, and `constexpr` code does not always run.**
  Constructors evaluated by the compiler and template instantiations used only
  in `static_assert`s report as uncovered lines; the fix (running the same
  code at run time as well) is also a test that the two evaluations agree.
- **The system suite is the right place to measure firmware coverage.** The
  instrumented `native_sim` build costs one extra build and shows which
  firmware paths the suite actually drives, which a unit test of the glue
  could not have told.
- **Measure the instrument before trusting it.** Two runs of the same suite
  reporting 88 % and 85 % firmware coverage were not the suite changing but
  coverage data lost at exit; the exit time of an instrumented process (2.5 s
  against 5 ms) was the number that explained it.
- **Skip is a verdict.** An inconclusive run on a loaded desktop is neither a
  pass nor a firmware failure; recording it as a skip with the stall and the
  failed checks in the reason keeps the report honest and the retry visible.

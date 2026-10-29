# Phase 7 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2,
image `can-ecu-sim-dev:local` with rustc and cargo 1.89.0, the
`x86_64-unknown-linux-gnu` and `thumbv7em-none-eabihf` targets, clippy and
rustfmt). Every command ran through `tools/dev.sh` inside the dev image. Per
the review policy, the `no_std` crate had the reduced adversarial review (one
finder, one verifier per finding, as in Phase 3); the adapter, the generator
and the tooling had none.

## 1. Generators and vectors

`can/gen_test_vectors.py` was split into a collector (`collect_vectors`, one
seeded pass over the DBC) and the C++ emitter, and `can/gen_rust.py` added as
the second emitter over the same data plus the codec generator for Rust.
`tools/gen-all.sh` regenerated everything: `git diff` of
`firmware/lib/protocol/tests/generated/test_vectors.hpp` was empty, so the
C++ side is bit-for-bit what Phase 1 committed; `rust/sensor-core/src/generated/messages.rs`
came out at 1124 lines (9 messages, 6 enums) and
`rust/sensor-core/tests/generated/vectors.rs` at 450 (9 × 32 message vectors,
64 CRC vectors, 20 protected frames, 6 enum tables).

Two generator fixes on the way: the inner attributes (`#![allow(...)]`) the
first version wrote at the top of each generated file are a parse error in a
file pulled in with `include!`, so the allows moved to the include sites
(`lib.rs`, `tests/conformance.rs`); and the emitted `as u32` on 32-bit unsigned
signals was a clippy finding (`unnecessary_cast`), so the generator now casts
only where the field width differs from the codec's word.

## 2. The core crate

The whole workspace (nine hand-written modules, the generated codec, the
application core, the adapter) compiled at the first attempt with one warning
(an unused import), 46 s in the fresh container including the crate
downloads. `tools/build-rust.sh` then ran four times to a clean gate:

1. clippy stopped on the workspace manifest before any code: a lint group and
   an individual allow at the same priority is an error
   (`lint_groups_priority`), which is also why five allows in the first run
   had no effect. `all` and `pedantic` moved to priority −1.
2. The real findings, each fixed in the code: `CounterGenerator::next` is
   confusable with `Iterator::next` (`should_implement_trait`, renamed
   `advance`); `6.283_185_3` is an approximation of `TAU` (`approx_constant`,
   the constant is used); `if !allowed {} else if ...` in `transmit()`
   (`if_not_else`, restructured); six `match` arms without a trailing
   semicolon (`semicolon_if_nothing_returned`); a test building a message by
   assigning a field after `default()` (`field_reassign_with_default`, struct
   update syntax); a `const` after a statement in the adapter
   (`items_after_statements`); two identical `match` arms in the receive loop
   (`match_same_arms`).
3. Of the 34 unit tests in the crate, 32 passed at once and two failed in the
   ported test rather than in the ported code: the counter-mask test asked
   for `0xF6` after counter 8 to be "skipped", but its masked value 6 is 14
   steps on, out of sequence in C++ too (now `0x1A`, counter 10, two on,
   skipped with one frame lost); and the protected-receiver test expected two
   consecutive rejects after an out-of-sequence, a CRC and a DLC reject in a
   row, which the C++ test counts as three. The six conformance tests against
   the shared vectors passed at the first run.
4. The release build of `sensor-vcan` (6.2 MB with debug info, 513 KB
   stripped; dependencies `socketcan` 3.6.2, `libc` 0.2.191 and the core) and
   the `thumbv7em-none-eabihf` build of `sensor-core` (an 867 KB `rlib` with
   `libm` 0.2.16 as its only dependency) completed; `Cargo.lock` is committed
   and every step runs `--locked`.

## 3. The adapter and the suite

`tools/smoke-test.sh rust/target/release/sensor-vcan` captured its five
protected heartbeats, and the first run of the unchanged suite against the
Rust sensor (the C++ controller opposite, the gateway between) passed 23 of 23
in 86.2 s, with the single test change of the phase already in place: the
watchdog test's needle `Booting Zephyr` widened to `*** Booting`, since the
adapter prints `*** Booting can-ecu-sim sensor ECU (Rust) ***` and not the
kernel's banner.

One value in that first run was on its limit: the liveness measurement of the
watchdog test (the time from the `thread blocked` log line to the first
`watchdog not fed`) read 290.0 ms against a 290..330 ms band, where the C++
sensor reads 295.0. The adapter had kept the firmware's loop order (stall
hook, then check-in), under which the stall begins one tick after the last
check-in; under Zephyr the supervisor's lower priority makes the resulting
verdict deterministic, under Linux two OS threads of equal priority raced it
onto either side of the window (both tick threads had been started at the
same instant, so their deadlines coincided every 10 ms). The receive and
transmit loops now check in before the stall hook (DESIGN.md section 16.4).
After the change the watchdog test was run three times on its own and the
whole suite once more:

| Run | feed withheld after the stall (limit 290..330 ms) | watchdog reset after the last frame (limit 1290..1330 ms) |
|---|---|---|
| first full run, firmware order | 290.0 ms | 1298.4 ms |
| watchdog test alone, after the change | 305.0 ms | 1315.4 ms |
| watchdog test alone, after the change | 304.9 ms | 1313.1 ms |
| full run, after the change (the run in RESULTS.md section 3) | 305.1 ms | 1313.7 ms |

The second full run passed 23 of 23 in 86.5 s; its measured values are
RESULTS.md section 3 and DESIGN.md section 16.7. The only changes outside
`rust/` and `can/gen_rust.py` are the `--results` option of
`tools/run-system-tests.sh` (so the two runs keep separate directories), the
needle above, the CI steps and artifact, the Rust rows of the job summary, and
the split of the vector generator.

## 4. Review of the no_std crate

Per the policy, one finder over `rust/sensor-core` (the hand-written modules,
the generated codec and its generator, with the C++ counterparts as the
reference) and one adversarial verifier per finding, a workflow of four agents
(618 k tokens, 16 min). The finder reported three findings, all low severity;
all three were confirmed by their verifiers and fixed before the commit:

1. **A suppressed frame counted as blocked by the simulated bus-off**
   (`sensor.rs`, `transmit()`). The firmware's suppression hook sits in the
   application's `transmit()` and decides before the frame reaches the HAL,
   where the simulated controller's bus-off block and its `blocked_bus_off`
   counter live; the port tested the simulated bus-off first. In the window
   where the simulated controller is bus-off but the CAN error state machine
   still allows transmission (up to 5 ms after `FORCE_BUS_OFF`, or for the
   whole stall when the supervisor thread is held by `STALL_THREAD`), a frame
   under `SUPPRESS_HEARTBEAT` or `SUPPRESS_MESSAGE` incremented
   `tx blocked_bus_off` instead of `hooks suppressed`. The frame was dropped
   either way; the two status counters diverged from the C++. Fixed by testing
   the suppression first, the firmware's order.
2. **Checked additions on the fault budgets** (`sensor.rs`, `test_control`).
   `CORRUPT_CRC`, `SKIP_COUNTER` and `FREEZE_COUNTER` added `arg0` (up to
   65 535) to a `u32` budget with a plain `+=`: 65 538 such frames overflow
   it, which the C++ wraps and a Rust build with overflow checks (every debug
   and test build) turns into a panic that takes the ECU down; the release
   build the suite runs wraps like the C++. The verifier extended the point to
   every `+= 1` event counter, which a debug build would trip after 2^32
   events (248 days of watchdog feeds). Fixed with saturating arithmetic on
   all 45 diagnostic counters and budgets of the crate: a budget that
   saturates cannot wrap to a small number either, which is the better
   semantics for a "frames remaining" count (the C++ wraps, a difference only
   an absurd input reaches). Loop indices and the error-counter model, which
   has its own limits, are untouched.
3. **The recovery event stamped at the tick, not at the completion**
   (`sensor.rs`, `complete_sim_recovery()`). The C++ HAL recovers the
   simulated controller from a delayed work item 3 ms after the request and
   stamps the `ERROR_ACTIVE` event with that instant; the port recovered it on
   the first 5 ms supervisor tick at or after the due time and stamped the
   event with the tick, 2 ms later: `last_recovery_ms`, `active_since_ms` and
   the recovery duration in the status read 105 ms where the C++ reads 103,
   and the stable window started 2 ms late. The crate's own test had pinned
   the wrong value. Fixed by stamping the event with the due time (the phase
   change still happens on the same tick); the test now expects 103 ms, as the
   C++ unit test of the same policy does.

After the fixes: rustfmt, clippy, the 34 unit and 6 conformance tests and
both builds clean; the full suite once more against the rebuilt Rust sensor,
23 of 23 in 84.7 s. The suite exercises none of the three paths (it never
suppresses a frame of a bus-off sensor, never sends a five-figure fault
budget, and reads the recovery duration of the controller only), which is
what made the review worth running where the suite had already passed.

## 5. The CI run of this phase

The run for the Phase 7 commit `8472b9b`
([40824884673](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40824884673))
failed after 11 min 10 s in step 25 of 33, the C++ system suite with firmware
coverage; every step before it had passed, the new Rust gate among them (31 s
for rustfmt, clippy, the 40 tests, the release build and the cross-compile),
and the Rust conformance run (step 26) was skipped as a consequence. The
failing check was `test_bus_off_recovery_after_the_backoff`'s "bus-off to
error-active in the firmware log (ECU clock) = 100.0 ms [100..118]": the
controller recovered exactly one backoff after the bus-off in its own log, a
legitimate alignment of the 5 ms supervisor tick with the event (105 ms in most
runs), and the harness, which builds its timestamps as floating-point seconds,
computed the difference a hair below 100 and lost it against the limit. The
number printed next to the verdict was right; the comparison was not.
`Measure.check()` now rounds a floating-point value to six decimals, the
microsecond resolution of the logs the values come from, before it meets its
limit; no expectation changed, and the rounding applies to both suite runs.
The C++ suite passed locally after the fix (23 of 23, 85 s); the CI run of the
fix commit is recorded with the next commit.

## 6. What Phase 7 taught

The full list is DESIGN.md section 16.8; in short: a core that takes time as
an argument is the cheapest test fixture there is; share the oracle between
the two codecs rather than re-deriving it; console compatibility is a
specification the unchanged suite then enforces; the OS shows through in the
one ordering that priorities had made deterministic; Cargo's lint-table
priorities are part of the policy; a failing ported test is a question about
the test first.

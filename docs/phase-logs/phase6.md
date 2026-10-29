# Phase 6 verification log

Same environment as before (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2,
image `can-ecu-sim-dev:local` with clang-tidy 18.1.3, cppcheck 2.13.0, CMake
3.28.3). Every command ran through `tools/dev.sh` inside the dev image. Per
the review policy, this phase (CI and tooling) had no adversarial review.

## 1. The Phase 5 CI run

The run for the Phase 5 commit `fd9f17d`
([40813161893](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40813161893))
passed all 27 steps in 8 min 0 s: image build and push 2.5 min, the four
firmware builds 85 s, the six `vcan` demo steps 72 s and the new system test
suite with firmware coverage 120 s (94 s locally).

## 2. Static analysis, first run

`tools/static-analysis.sh` on the Phase 5 code, with clang-tidy's full
`bugprone`, `cert`, `clang-analyzer`, `concurrency`, `cppcoreguidelines`,
`misc`, `modernize`, `performance`, `portability` and `readability` groups minus
the style checks `.clang-tidy` lists first (magic numbers, trailing return
types, identifier length, non-const globals, printf-style logging, C arrays,
the Zephyr API's unions): 341 findings from 19 checks over 19 translation
units (the eight application files in both roles, the ten unit-test files).
Grouped by origin:

| Findings | Check | Origin |
|---|---|---|
| 42 + 31 + 31 | `cppcoreguidelines-avoid-do-while`, `-pro-type-cstyle-cast`, `readability-simplify-boolean-expr` | every `LOG_*` call (31 sites) and `CODE_UNREACHABLE`: the macro's `do { } while (0)`, its C casts, its level test |
| 15 + 8 | `cppcoreguidelines-pro-bounds-array-to-pointer-decay`, `-pro-type-union-access` | `can_frame::data`, a C array in a union, handed to the codec |
| 14 | `misc-use-anonymous-namespace` | `LOG_MODULE_REGISTER`'s file-scope statics |
| 10 | `clang-diagnostic-ignored-optimization-argument` | clang on the gcc command line |
| 2 | `misc-header-include-cycle` | Zephyr's `kernel.h` / `tracing/tracking.h` |
| 65 | `modernize-use-auto` | explicit integer widths in the protocol code (kept on purpose) |
| 56 | `misc-const-correctness` | 46 `Locked` guards, 5 test locals |
| 27 | `cppcoreguidelines-pro-type-member-init` | the generated test-vector structs (9 structs, 3 units) |
| 15 | `readability-avoid-nested-conditional-operator` | two clamps |
| 6 + 5 + 5 + 3 + 3 + 2 + 1 | `missing-std-forward`, `special-member-functions`, `performance-enum-size`, `named-parameter`, `bugprone-branch-clone`, `cert-err33-c`, `pro-type-reinterpret-cast` | one site each, seen through several units |

cppcheck did not run in this first attempt: its `--file-filter='*/firmware/*'`
matched nothing (the glob does not cross directories) and the filter was
dropped, the merged database already holding only units under `firmware/`.

The first group was turned off in `.clang-tidy` with its reason (the sources
have no `do/while`, no C cast and no boolean expression to simplify; the
checks fire inside Zephyr's expansions), `modernize-use-auto` likewise, clang's
optimisation-argument warning is silenced in the rewritten database, and the
rest was fixed or suppressed inline:
- Two nested conditional operators, the clamp in `codec::to_raw`
  (`codec_util.hpp`) and the controller's `clamp()`, rewritten as `if`
  chains (`readability-avoid-nested-conditional-operator`).
- The `Locked` spinlock guard declared a destructor and deleted copies but not
  moves (`cppcoreguidelines-special-member-functions`): the moves are deleted
  too. Its 46 uses are `const Locked l(...)` now, the same advice clang-tidy
  gives for `std::lock_guard` (`misc-const-correctness`).
- `ThreadId` was an `enum class : std::size_t` with three values
  (`performance-enum-size`): `std::uint8_t`.
- The three thread entry functions had unnamed parameters
  (`readability-named-parameter`): named as the unused `p1..p3` Zephyr passes.
- Both roles discarded `snprintf`'s length when formatting the status line
  (`cert-err33-c`): acknowledged with a cast and a comment, truncation of a
  diagnostic line being acceptable.
- The generated test-vector structs left their arrays uninitialised
  (`cppcoreguidelines-pro-type-member-init`): the generator
  (`can/gen_test_vectors.py`) emits default member initialisers, the header
  was regenerated and the `--check` in CI keeps them in step.
- A `reinterpret_cast` from `const char*` in the CRC test
  (`cppcoreguidelines-pro-type-reinterpret-cast`): a byte vector instead.
- Five locals in the tests that could be `const` (`misc-const-correctness`).

Suppressed inline, each with its reason: `TimeoutMonitor::tick()` takes its callback as a
forwarding reference and calls it once per lost entry without
`std::forward` (`cppcoreguidelines-missing-std-forward`; forwarding would move
from it inside the loop); five test lines cast an out-of-table value to an
enum on purpose, to reach the `"UNKNOWN"` branch of `to_string`
(`clang-analyzer-optin.core.EnumCastOutOfRange`); and the shell command table
in `shell_cmds.c` is wrapped in a `NOLINTBEGIN(bugprone-branch-clone)`, the
identical branches being inside Zephyr's `SHELL_CMD` expansion.

## 3. Static analysis, clean

Three more iterations to a clean run:

1. After the fixes, clang-tidy was down to one finding the first run had not
   shown (it was behind the macro-expansion noise): `clang-analyzer-deadcode`
   on `run_node_state()`'s `const char* reason = "";`, a value overwritten
   under the lock before any read. Initialised to `nullptr` instead, which the
   checker treats as the idiom it is.
2. cppcheck, now running, reported seven items: six `useStlAlgorithm`
   suggestions (`std::find_if`, `std::all_of`, `std::fill`) on loops in the
   constexpr libraries and one test, which C++17 cannot follow (its algorithms
   are not constexpr); the check is off with that reason. The seventh was its
   own: `failed to expand 'IS_ENABLED', wrong number of parameters for macro
   'Z_IS_ENABLED2'` from Zephyr's `sys/util_internal.h`. Feeding it gcc's
   predefined macros and the build's `autoconf.h` did not change that; the
   comma-pasting trick behind `IS_ENABLED()` defeats cppcheck 2.13's
   preprocessor. The consequence mattered more than the message: a
   deliberately planted `redundantAssignment` in `ecu.cpp` went unreported,
   so every unit that includes a Zephyr header was being skipped, and the
   "clean" verdict for the application would have been empty.
3. The next attempt fed cppcheck the units as the real compiler preprocesses
   them (`-E`, line markers kept), which does get past the macro: 26 of 26
   units preprocessed, and findings appeared in application files. They were
   on the wrong lines. cppcheck reported the module's log-level assignment
   (line 8 of `watchdog.cpp`, inside `LOG_MODULE_REGISTER`) at line 23, log
   macro internals (`string_ok`, `__log_level`, a C cast) at lines that hold
   unrelated statements, and a syntax error at "line 11930" of a 400-line
   test file: it mis-tracks the compiler's line markers, and GoogleTest's
   expanded code defeats its parser. With or without `-C`. Locations that
   cannot be trusted make a report that cannot be acted on, so the attempt
   was dropped.
4. cppcheck therefore covers what it parses correctly: the library headers
   through their ten unit-test units, in project mode from the host build's
   database (plain g++ command lines, no Zephyr), with GoogleTest's `TEST()`
   given a parsable shape. Over those it is clean. The application sources
   are clang-tidy's alone; clang parses them in full, in both roles.
5. The final run as CI performs it: clang-tidy clean over 19 translation units
   (26 compile commands, the shared files once per role), cppcheck clean over
   the 10 library-test units, all four firmware images built, 77 GoogleTests
   passed at 100 % line coverage of the library headers.

## 4. Reports, artifacts, summary

The workflow gained, in order: a `concurrency` group that cancels a run a
newer push supersedes; `--junitxml=build/test-results/unit-tests.xml` on the
Python unit suites; `--output-junit` on the CTest run of the C++ tests
(`build/host-tests/host-tests.xml`, written by `tools/run-host-tests.sh`,
which now also exports the compile database the static analysis needs); the
step `Static analysis, clang-tidy and cppcheck, every finding an error` after
the allocator check; and three artifact changes: `host-tests` (JUnit and
coverage, replacing `host-test-coverage`), `static-analysis` (the clang-tidy
log, the cppcheck XML and text) and `test-results` (replacing `system-tests`:
the JUnit of the unit suites and the system suite, `measurements.json` and
`.md`, the logs of every module, the firmware coverage). A last step,
`tools/ci-summary.py`, reads those files and appends to the job summary the
test counts of every suite, both coverage figures, the static-analysis verdict
and, folded, the system suite's measured-value table; it reports a missing
file as missing, so a run that failed early still has a summary.
`.github/dependabot.yml` keeps the actions at their current majors' latest
releases, and the README carries the workflow badge.

## 5. The CI run of this phase

The run for the Phase 6 commit `8d34d3e`
([40818484542](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40818484542))
passed all 30 steps in 14 min 9 s. The image build took 5 min 0 s this time
(the GitHub layer cache did not serve the Phase 5 layers; the push 59 s), the
four firmware builds 98 s, the new static-analysis step 2 min 23 s (clang-tidy
over 26 compile commands and cppcheck over the 10 library-test units, both
clean), the six `vcan` demo steps 72 s and the system test suite with firmware
coverage 2 min 6 s, with every artifact uploaded and the job summary written.
The same push also started the two Dependabot runs the new
`.github/dependabot.yml` asks for (68 s each, both successful), which is why
the run list of the repository now shows three runs per push.

## 6. What Phase 6 taught

- **Triage by origin before counting.** 341 findings looked like a lot; three
  quarters of them were one Zephyr macro (`LOG_*`) seen through four checks at
  31 call sites, and none of those constructs exist in the sources. Grouping
  the first run by check and by file turned a wall of errors into eight real
  fixes and three suppressions.
- **The configuration is documentation.** A disabled check without a reason
  is a hidden decision; `.clang-tidy` lists each one with its reason and the
  inline suppressions carry theirs, so a reviewer can disagree with a line
  rather than with a list.
- **Generated code is code.** The test-vector header was fixed in its
  generator, not by hand; the `--check` step keeps it that way.
- **Feed the analysers the compiler's view.** A filtered copy of the real
  compile database (both roles, the host tests) is what makes clang-tidy's
  verdict about the firmware rather than about a guess at its defines;
  cppcheck needs `autoconf.h` force-included because it ignores `-imacros`,
  and its file filter does not cross directories.
- **Every finding an error, from the first green run.** With
  `WarningsAsErrors: '*'` and `--error-exitcode=1` the analysis cannot drift:
  a new finding is a failed build, and the choice between fixing it and
  suppressing it with a reason is made in the pull request that introduced it.

# Phase 8 verification log

The last phase changed no code and no test; it finished the three documents
and checked that what they say is what the runs recorded. Per the review
policy, no adversarial review.

## 1. What was written

- `README.md` in its final form: the architecture diagram (Mermaid, rendered
  by GitHub), a description of each part with a pointer into DESIGN.md, the
  results table with one command and one raw-data link per row (33 rows, the
  28 of Phase 7 plus line coverage, static analysis, the Rust gate and the
  CI pipeline), the quick start unchanged from Phase 7, the layout and a
  documentation map.
- `RESULTS.md`: a section 0 that lists the seven runs the README quotes
  (command, date, commit, where recorded, what reproduced it), maps every
  README row to its run and section, and tabulates the six CI runs of the
  project; sections 2.4 and 3.2 with the runner's own `measurements.md` of
  both suite runs from the final CI run's artifacts (`test-results`,
  `test-results-rust` of run 40826567803), each with a side-by-side table of
  twenty values against the local run the README quotes.
- `DESIGN.md`: the contents completed (0 to 17), the architecture and layout
  sections rewritten as built, the CI paragraph closed, the phase table given
  a column that says where each exit criterion is met, and section 17 (what
  Phase 8 changed, how a number is traced, what the whole project taught, the
  limits, the CI runs).

## 2. What was checked

- **Links and anchors.** A script walked every relative link in the three
  documents, `docs/fault-model.md` and the eight phase logs (59 links),
  checked that the file exists and, for a fragment, that the target document
  has a heading with that GitHub-style anchor (lowercase, punctuation
  stripped, spaces to hyphens, duplicates suffixed). The first pass found one
  broken anchor, the README's link to section 17 before the section existed;
  the final pass found none.
- **Numbers.** A second script took every numeric token of the README
  table's "Measured" column (33 rows) and required it to appear verbatim in
  `RESULTS.md`, `DESIGN.md` or a phase log. All of them do: the rows were
  built from the recorded tables, not retyped.
- **The runner's reproduction.** The twenty values compared in RESULTS.md
  sections 2.4 and 3.2 behave as section 0 of RESULTS.md says they should:
  the firmware-clock values (detection times, hold times, the watchdog, the
  error-passive and bus-off intervals) agree with the local runs to within a
  supervisor tick or a publish period, and the host-side values (period
  jitter, start-up, reboot) differ by the runner's scheduling, the runner's
  SENSOR_DATA period standard deviation being 0.059 ms (C++) and 0.021 ms
  (Rust) against 0.109 and 0.050 ms locally.
- **The diagram.** Checked rendered on the repository's front page on GitHub
  after the push (recorded in section 3).

## 3. The CI run of this phase and the rendering

Recorded after the run completed, in the commit that follows this phase's.

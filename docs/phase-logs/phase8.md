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

The run for the Phase 8 commit `1d54d38`
([40901551354](https://github.com/dcgursoy/can-ecu-sim/actions/runs/40901551354))
passed all 33 steps in 10 min 39 s, the fastest full run of the project
(image build 84 s and push 51 s, the four firmware builds 71 s, the static
analysis 97 s, the Rust gate 25 s, the C++ suite with firmware coverage
114 s, the suite against the Rust sensor 85 s), a documentation-only commit
exercising every step once more.

The README's diagram was checked on GitHub's rendering of the repository
front page: the Mermaid container rendered without an error notice and its
frame grew from the 180 px loading placeholder to 438 px once the diagram was
drawn. The same source was also rendered by an independent service
(mermaid.ink, which answers a bad diagram with an error): an SVG of 1923 by
593 units with the nine nodes, the one cluster, thirteen edges and every
label present.

## 4. Addendum: the demo recording and the figures

Added on 2026-10-30 after the question "is there a final demo or visual":
`tools/demo.py` (the narrated heartbeat-loss scenario), `tools/record_cast.py`
and `tools/record-demo.sh` (the recording under a pseudo-terminal and the
GIF), `tools/plots.py` (the four figures), `docs/figures/`, the README's
Demo and Figures sections, DESIGN.md section 17.6, RESULTS.md section 0 (run
H and the figures' sources), a CI step that regenerates all of it from the
run's own captures and uploads it, and two tools in the image (matplotlib
3.11, `agg` 1.9.0 with its SHA-256 pinned, plus the DejaVu font `agg` needs),
appended as a late Dockerfile layer so the Zephyr and Rust layers stayed
cached (the local rebuild took the new layer only).

What was checked:

- **The demo narrates the capture, not a script.** Its first run (`--fast`,
  no pauses) printed the boot times, the five rates, the decoded values, a
  CRC check of 180 protected frames, and the four events of the fault with
  their intervals (365 and 524 ms against the suite's 374.8 and 513.9 ms of
  the same scenario, within one command period), the controller's log lines
  and its status with `safe_state_entries=1` and one heartbeat timeout.
- **The recording.** The first attempt failed on the recorder's own argument
  parsing (`argparse.REMAINDER` swallowed the recorder's options); fixed by
  splitting the command line at `--` by hand. The cast holds 19 output
  events over 7.9 s; `agg` renders 14 frames at 110 by 36 cells, 316 KB. Two
  frames and the last one were extracted with Pillow and read: the colours,
  the values and the wrapping are as intended (two long event lines were
  shortened after the first recording wrapped them).
- **The figures.** Each PNG copy was read after drawing. The watchdog figure's
  first version picked the module's longest `SENSOR_DATA` gap, which is the
  killed-and-restarted sensor's 1533 ms, not the watchdog's; it is anchored on
  the `STALL_THREAD` command now and reads 1312 ms (C++) and 1323 ms (Rust),
  the suite's 1313.2 and 1323.3 ms. The heartbeat-loss figure reads the
  recorded demo's own intervals (its footer names the capture), within a
  command period of the suite's; the period histograms give the suite's
  numbers (C++ std 0.068 ms, Rust 0.045 ms on these captures); the echo
  figure matches RESULTS.md section 1 (p50 0.55 and 0.58 ms, p99 1.15 and
  1.20 ms, 200 of 200 answered by each ECU). One annotation overlapped a row
  label and was moved; thin tick marks that looked two-coloured in the PNG
  were an anti-aliasing artefact of 0.6 px lines, drawn at 1 px now.
- **Links and numbers.** The link checker passes on the 64 relative links
  (the five new image links among them); the README table is unchanged and
  its 33 rows still trace.
- **The CI run of the addendum**, commit `9163adc`
  ([41024544832](https://github.com/dcgursoy/can-ecu-sim/actions/runs/41024544832)):
  all 35 steps passed in 14 min 37 s. The image build took 4 min 34 s, the
  new layer (the font, `agg`, matplotlib) being built once on the runner; the
  new step, the demo recorded under a pseudo-terminal and the four figures
  drawn from the run's own captures, took 11 s, and its `demo-and-figures`
  artifact holds the runner's cast, GIF, capture and figures.

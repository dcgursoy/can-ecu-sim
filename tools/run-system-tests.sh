#!/usr/bin/env bash
# Run the system test suite (tests/) against the two-ECU network on vcan.
#
#   tools/run-system-tests.sh [--coverage] [--results DIR] [pytest args...]
#   tools/run-system-tests.sh -k bus_off -x
#   tools/run-system-tests.sh --results build/test-results-rust --sensor-exe rust/target/release/sensor-vcan
#
# Runs inside the dev container. Results land in build/test-results/ (or the
# --results directory; the run against the Rust sensor ECU keeps its own):
# system-tests.xml (JUnit, with every measured value as a property),
# measurements.json and measurements.md (every measured value next to its
# verdict), system/<test module>/ (the ECU, gateway and candump logs of each
# module) and, with --coverage, firmware-coverage.{txt,html,xml}: the line
# coverage of firmware/app/src under the suite, from gcov-instrumented
# native_sim builds (build/native_sim_<role>_cov, built here if missing or
# stale). Without --coverage the suite runs the normal builds, which must exist
# (tools/build-ecu.sh native_sim <role>).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
RESULTS="build/test-results"

COVERAGE=0
PYTEST_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --coverage) COVERAGE=1 ;;
        --results) RESULTS="$2"; shift ;;
        --results=*) RESULTS="${1#--results=}" ;;
        *) PYTEST_ARGS+=("$1") ;;
    esac
    shift
done

mkdir -p "$RESULTS"
EXE_ARGS=()
if [[ "$COVERAGE" -eq 1 ]]; then
    for role in sensor controller; do
        dir="build/native_sim_${role}_cov"
        # Rebuild when the sources are newer than the executable (west is incremental).
        tools/build-ecu.sh native_sim "$role" --coverage
        find "$dir" -name '*.gcda' -delete
    done
    EXE_ARGS=(--sensor-exe build/native_sim_sensor_cov/zephyr/zephyr.exe
              --controller-exe build/native_sim_controller_cov/zephyr/zephyr.exe)
else
    for role in sensor controller; do
        exe="build/native_sim_${role}/zephyr/zephyr.exe"
        if [[ ! -x "$exe" ]]; then
            echo "$exe not found; building it" >&2
            tools/build-ecu.sh native_sim "$role"
        fi
    done
fi

set +e
python3 -m pytest tests -v --junitxml="$RESULTS/system-tests.xml" --results-dir "$RESULTS" \
    ${EXE_ARGS[@]+"${EXE_ARGS[@]}"} ${PYTEST_ARGS[@]+"${PYTEST_ARGS[@]}"}
rc=$?
set -e

if [[ "$COVERAGE" -eq 1 ]]; then
    # The ECU processes wrote their .gcda files at exit (a SIGKILLed process writes
    # none; the suite restarts it and the new process adds its own at its exit).
    # Only the application's object directories are searched: the instrumented
    # Zephyr kernel and driver objects (over a hundred per build tree) would each
    # cost a gcov run, minutes on a bind mount, for data --filter discards anyway.
    gcovr --root . --filter 'firmware/app/src/' \
        --print-summary \
        --txt "$RESULTS/firmware-coverage.txt" \
        --html-details "$RESULTS/firmware-coverage.html" \
        --xml "$RESULTS/firmware-coverage.xml" \
        build/native_sim_sensor_cov/CMakeFiles/app.dir build/native_sim_controller_cov/CMakeFiles/app.dir
    echo "firmware coverage report: $RESULTS/firmware-coverage.txt"
fi
exit $rc

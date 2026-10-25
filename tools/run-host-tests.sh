#!/usr/bin/env bash
# Build and run the host unit tests of the protocol library with line coverage.
#
#   tools/run-host-tests.sh [build-dir]
#
# Produces <build-dir>/coverage.txt, coverage.html (with per-line details) and
# coverage.xml (Cobertura, for CI), and prints the gcovr summary. Runs inside
# the dev container (GoogleTest, gcovr).
set -euo pipefail

BUILD_DIR="${1:-build/host-tests}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

cmake -S firmware/lib/protocol -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DPROTOCOL_COVERAGE=ON
# Profile data left by a previous run of a since-recompiled test would be
# rejected ("different checksum"); gtest_discover_tests runs the binary at
# build time, so the cleanup has to happen before the build.
if [[ -d "$BUILD_DIR" ]]; then
    find "$BUILD_DIR" -name '*.gcda' -delete
fi
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure

# Coverage of the library headers only (the tests and GoogleTest are excluded).
# With -fkeep-inline-functions every translation unit emits its own copy of a
# header function and may report it at a different line; merge by lowest line.
gcovr --root . --object-directory "$BUILD_DIR" \
    --filter 'firmware/lib/protocol/include/' \
    --merge-mode-functions merge-use-line-min \
    --print-summary \
    --txt "$BUILD_DIR/coverage.txt" \
    --html-details "$BUILD_DIR/coverage.html" \
    --xml "$BUILD_DIR/coverage.xml"
echo "coverage report: $BUILD_DIR/coverage.txt"

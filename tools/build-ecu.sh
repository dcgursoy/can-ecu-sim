#!/usr/bin/env bash
# Build one firmware variant.
#
#   tools/build-ecu.sh <board> <role> [--coverage] [west build args...] [-- cmake args...]
#   tools/build-ecu.sh native_sim sensor
#   tools/build-ecu.sh native_sim sensor --coverage
#   tools/build-ecu.sh nucleo_f446re controller -p always
#   tools/build-ecu.sh nucleo_f446re controller -- -DCONFIG_APP_WATCHDOG_TIMEOUT_MS=2000
#
# Output always lands in build/<board>_<role>/ (ecu_network.py, check-no-heap.sh
# and CI depend on that path, so -d/--build-dir is refused). --coverage
# (native_sim only) builds a variant whose application objects are instrumented
# with gcov (CAN_ECU_APP_COVERAGE in firmware/app/CMakeLists.txt; the kernel is
# not) into build/<board>_<role>_cov/: the process writes its .gcda files into
# that directory when it exits, which tools/run-system-tests.sh turns into line
# coverage of the firmware under the system tests. Runs inside the dev container.
set -euo pipefail

BOARD="${1:?board (native_sim | nucleo_f446re)}"
ROLE="${2:?role (sensor | controller)}"
shift 2
case "$ROLE" in
    sensor) ROLE_SYM=CONFIG_APP_ROLE_SENSOR ;;
    controller) ROLE_SYM=CONFIG_APP_ROLE_CONTROLLER ;;
    *) echo "unknown role '$ROLE' (sensor | controller)" >&2; exit 2 ;;
esac

# west consumes only the first `--` and hands everything after it to CMake, so
# the role define and any user -D options must share one separator.
WEST_ARGS=()
CMAKE_ARGS=()
VARIANT=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --) shift; CMAKE_ARGS=("$@"); break ;;
        --coverage)
            if [[ "$BOARD" != native_sim* ]]; then
                echo "--coverage is for native_sim builds (gcov on the host)" >&2; exit 2
            fi
            VARIANT="_cov"
            CMAKE_ARGS+=(-DCAN_ECU_APP_COVERAGE=ON)
            shift ;;
        -d|--build-dir|-d=*|--build-dir=*)
            echo "the build directory is fixed to build/<board>_<role>; do not pass $1" >&2; exit 2 ;;
        *) WEST_ARGS+=("$1"); shift ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
# The board name may carry qualifiers (native_sim/native/64); keep the directory name flat.
BUILD_DIR="build/$(echo "$BOARD" | tr '/' '_')_${ROLE}${VARIANT}"

west build -b "$BOARD" -s firmware/app -d "$BUILD_DIR" ${WEST_ARGS[@]+"${WEST_ARGS[@]}"} \
    -- "-D${ROLE_SYM}=y" ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}
echo "built $BUILD_DIR"

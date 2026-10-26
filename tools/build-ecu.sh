#!/usr/bin/env bash
# Build one firmware variant.
#
#   tools/build-ecu.sh <board> <role> [west build args...] [-- cmake args...]
#   tools/build-ecu.sh native_sim sensor
#   tools/build-ecu.sh nucleo_f446re controller -p always
#   tools/build-ecu.sh nucleo_f446re controller -- -DCONFIG_APP_WATCHDOG_TIMEOUT_MS=2000
#
# Output always lands in build/<board>_<role>/ (ecu_network.py, check-no-heap.sh
# and CI depend on that path, so -d/--build-dir is refused). Runs inside the dev
# container.
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
while [[ $# -gt 0 ]]; do
    case "$1" in
        --) shift; CMAKE_ARGS=("$@"); break ;;
        -d|--build-dir|-d=*|--build-dir=*)
            echo "the build directory is fixed to build/<board>_<role>; do not pass $1" >&2; exit 2 ;;
        *) WEST_ARGS+=("$1"); shift ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
# The board name may carry qualifiers (native_sim/native/64); keep the directory name flat.
BUILD_DIR="build/$(echo "$BOARD" | tr '/' '_')_${ROLE}"

west build -b "$BOARD" -s firmware/app -d "$BUILD_DIR" ${WEST_ARGS[@]+"${WEST_ARGS[@]}"} \
    -- "-D${ROLE_SYM}=y" ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}
echo "built $BUILD_DIR"

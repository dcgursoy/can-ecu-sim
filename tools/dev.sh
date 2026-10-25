#!/usr/bin/env bash
# Run a command inside the dev container image from a Linux or Git Bash host.
#
#   tools/dev.sh                      # interactive shell
#   tools/dev.sh west build ...       # one command
#   tools/dev.sh --build              # (re)build the image first
#
# The repository is bind-mounted at /workspaces/can-ecu-sim and the container
# gets CAP_NET_ADMIN so tools/vcan-up.sh can create the virtual CAN interfaces.
# Every invocation is a fresh container with its own network namespace, so
# multi-process work (two ECUs plus candump) belongs in one interactive shell.
#
# Environment:
#   CAN_ECU_SIM_IMAGE        image to run (default can-ecu-sim-dev:local)
#   CAN_ECU_SIM_DOCKER_ARGS  extra `docker run` flags, e.g. "--user root" in CI
#                            where the checkout is owned by a different uid than
#                            the image's dev user
set -euo pipefail

IMAGE="${CAN_ECU_SIM_IMAGE:-can-ecu-sim-dev:local}"
PLATFORM="linux/amd64"   # the Zephyr SDK tarball and gcc-multilib are x86_64 only
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST_OS="$(uname -s)"

if [[ "${1:-}" == "--build" ]]; then
    shift
    BUILD_ARGS=()
    if [[ "$HOST_OS" == "Linux" ]]; then
        # Docker Engine does no uid translation on bind mounts: give the image's
        # dev user the host uid/gid so builds can write into the checkout.
        BUILD_ARGS+=(--build-arg "USER_UID=$(id -u)" --build-arg "USER_GID=$(id -g)")
    fi
    docker build --platform "$PLATFORM" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"} \
        -t "$IMAGE" -f "$REPO_ROOT/.devcontainer/Dockerfile" "$REPO_ROOT"
    [[ $# -gt 0 ]] || exit 0
fi

HOST_REPO="$REPO_ROOT"
case "$HOST_OS" in
    MINGW*|MSYS*|CYGWIN*)
        # Git Bash rewrites POSIX-looking arguments into Windows paths; disable
        # that for the docker invocation and hand docker a Windows path.
        HOST_REPO="$(cygpath -w "$REPO_ROOT")"
        export MSYS_NO_PATHCONV=1
        # Docker Desktop's WSL2 VM forgets the out-of-tree vcan module whenever
        # it restarts; reinstall it from the cached build when it is missing.
        # (stdin is redirected so the probe cannot swallow input meant for the
        # command, e.g. a here-document piped into the container.)
        if ! wsl.exe -d docker-desktop -e grep -q '^vcan ' /proc/modules </dev/null 2>/dev/null; then
            "$REPO_ROOT/tools/wsl2/install-vcan-module.sh" </dev/null \
                || echo "warning: vcan is not available in the Docker VM; see tools/wsl2/" >&2
        fi
        ;;
    Linux)
        IMAGE_UID="$(docker image inspect --format '{{ index .Config.Labels "can-ecu-sim.uid" }}' "$IMAGE" 2>/dev/null || true)"
        if [[ -n "$IMAGE_UID" && "$IMAGE_UID" != "$(id -u)" && -z "${CAN_ECU_SIM_DOCKER_ARGS:-}" ]]; then
            echo "warning: image user uid $IMAGE_UID differs from host uid $(id -u); builds may not be able" >&2
            echo "         to write to the checkout. Rebuild with 'tools/dev.sh --build' or set" >&2
            echo "         CAN_ECU_SIM_DOCKER_ARGS='--user root'." >&2
        fi
        ;;
esac

TTY_FLAGS="-i"
if [[ -t 0 && -t 1 ]]; then
    TTY_FLAGS="-it"
fi

# shellcheck disable=SC2086
exec docker run --rm $TTY_FLAGS \
    --platform "$PLATFORM" \
    --cap-add=NET_ADMIN \
    --hostname can-ecu-sim \
    -v "${HOST_REPO}:/workspaces/can-ecu-sim" \
    -w /workspaces/can-ecu-sim \
    ${CAN_ECU_SIM_DOCKER_ARGS:-} \
    "$IMAGE" "$@"

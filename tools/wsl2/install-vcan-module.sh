#!/usr/bin/env bash
# Make `ip link add type vcan` work on Docker Desktop (WSL2 backend) on Windows.
#
# Usage (Git Bash):  tools/wsl2/install-vcan-module.sh [--rebuild]
# PowerShell/cmd:    tools\wsl2\install-vcan-module.cmd [-Rebuild]
#
# Steps:
#   1. Ask the Docker daemon which kernel the VM runs.
#   2. If tools/wsl2/out/<kernel>/vcan.ko is missing (or --rebuild was given),
#      build it in a throwaway privileged ubuntu container
#      (see build-vcan-module.sh, about five minutes).
#   3. Copy it into the VM's /lib/modules, run depmod and load it, so the kernel
#      can also auto-load it on demand from a plain NET_ADMIN container.
#   4. Verify by creating a vcan interface inside a NET_ADMIN container.
#
# Persistence: the VM's module directory is an overlay whose writable layer lives
# in RAM, so the installed module disappears whenever the VM itself reboots
# (`wsl --shutdown`, a Windows reboot, a Docker Desktop update). Stopping and
# starting Docker Desktop alone keeps the VM, and the module with it. After a
# reboot this script is a ten-second copy + depmod + modprobe from the cached
# build; tools/dev.sh and tools/dev.ps1 run it automatically when the module is
# missing. A WSL kernel update changes the kernel release and forces a rebuild,
# which the cache directory name detects.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$HERE/out"
REBUILD=0
[[ "${1:-}" == "--rebuild" ]] && REBUILD=1

# Git Bash would otherwise rewrite /lib/modules into C:\Program Files\Git\lib\modules.
export MSYS_NO_PATHCONV=1
if command -v cygpath >/dev/null 2>&1; then
    HOST_HERE="$(cygpath -w "$HERE")"
else
    HOST_HERE="$HERE"
fi

KREL="$(docker info --format '{{.KernelVersion}}')"
echo "==> Docker VM kernel: $KREL"
case "$KREL" in
    *-microsoft-standard-WSL2*) ;;
    *linuxkit*) echo "This is Docker Desktop's LinuxKit VM (macOS/Hyper-V); no vcan recipe exists for it yet." >&2; exit 1 ;;
    *) echo "This host is not Docker Desktop on WSL2. Try: sudo modprobe vcan" >&2; exit 1 ;;
esac

# BusyBox's ip supports "type vcan", so a bare alpine image is enough for probing.
probe() {
    docker run --rm --cap-add=NET_ADMIN alpine:3.20 \
        sh -c 'ip link add dev vcanprobe type vcan 2>/dev/null'
}

if [[ $REBUILD -eq 0 ]] && probe; then
    echo "==> vcan already works in this kernel; nothing to do."
    exit 0
fi

KO="$OUT_DIR/$KREL/vcan.ko"
if [[ ! -f "$KO" || $REBUILD -eq 1 ]]; then
    echo "==> building vcan.ko for $KREL (this takes several minutes)"
    mkdir -p "$OUT_DIR"
    docker run --rm --privileged \
        -v "${HOST_HERE}/out:/out" \
        -v "${HOST_HERE}/build-vcan-module.sh:/build-vcan-module.sh:ro" \
        ubuntu:24.04 bash /build-vcan-module.sh
fi
[[ -f "$KO" ]] || { echo "error: $KO was not produced" >&2; exit 1; }

echo "==> installing into the VM's /lib/modules/$KREL and loading"
docker run --rm --privileged \
    -v /lib/modules:/lib/modules \
    -v "${HOST_HERE}/out:/out:ro" \
    alpine:3.20 sh -euc '
        KREL="$(uname -r)"
        DEST="/lib/modules/$KREL/kernel/drivers/net/can"
        # A previously loaded copy (e.g. before --rebuild) is replaced only if unused.
        rmmod vxcan vcan 2>/dev/null || true
        mkdir -p "$DEST"
        cp "/out/$KREL/vcan.ko" "/out/$KREL/vxcan.ko" "$DEST/"
        depmod -a "$KREL"
        modprobe can && modprobe can-raw && modprobe can-dev && modprobe can-gw
        modprobe vcan && modprobe vxcan
        echo "loaded CAN modules:"; grep -E "^(can|can_raw|can_dev|can_gw|vcan|vxcan) " /proc/modules
        echo "taint: $(cat /proc/sys/kernel/tainted) (4096 = out-of-tree module flag only)"'

echo "==> verifying from an unprivileged NET_ADMIN container"
docker run --rm --cap-add=NET_ADMIN alpine:3.20 sh -euc \
    'ip link add dev vcan0 type vcan && ip link set up vcan0 && ip link show vcan0'
echo "==> done. vcan is available to containers started with --cap-add=NET_ADMIN."

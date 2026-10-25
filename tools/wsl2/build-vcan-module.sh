#!/usr/bin/env bash
# Build the vcan (and vxcan) kernel modules for the kernel this container runs on.
#
# Meant to be run INSIDE a throwaway privileged ubuntu container started by
# install-vcan-module.{sh,ps1}; it needs /proc/config.gz of
# the running kernel and writes the modules to /out/<kernel-release>/.
#
# Why this exists: Docker Desktop on Windows runs containers on Microsoft's WSL2
# kernel, which ships CAN core, can-raw, can-dev and can-gw as modules but has
# CONFIG_CAN_VCAN disabled. The module source is in the public WSL2 kernel tree,
# so we build just drivers/net/can against the exact source tag and config of the
# running kernel. Module signing is off in that kernel (CONFIG_MODULE_SIG unset).
#
# About CONFIG_MODVERSIONS: Microsoft does not publish Module.symvers for the WSL2
# kernel and 6.x kernels no longer expose __crc_* symbols through kallsyms, so a
# module built here only records CRCs for the symbols it imports from the modules
# built next to it (can-dev). For the remaining kernel symbols the loader finds
# no recorded CRC and, by design (kernel/module/version.c), logs one warning and
# accepts the module. ABI compatibility is guaranteed by building from the exact
# source tag and .config of the running kernel; vermagic is checked as usual.
set -euo pipefail

KREL="$(uname -r)"                              # 6.6.87.2-microsoft-standard-WSL2
case "$KREL" in
    *-microsoft-standard-WSL2*) ;;
    *) echo "error: this is not a WSL2 kernel ($KREL); on native Linux use 'sudo modprobe vcan'" >&2; exit 1 ;;
esac
KBASE="${KREL%%-microsoft-standard-WSL2*}"     # 6.6.87.2
TAG="linux-msft-wsl-${KBASE}"
OUT="/out/${KREL}"
JOBS="$(nproc)"

echo "==> kernel ${KREL}, source tag ${TAG}"
mkdir -p "$OUT" /build

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential bc bison flex libssl-dev libelf-dev dwarves kmod cpio \
    wget ca-certificates xz-utils python3 rsync >/dev/null

cd /build
if [[ ! -d src ]]; then
    echo "==> downloading kernel source ${TAG}"
    wget -q --show-progress -O src.tar.gz "https://github.com/microsoft/WSL2-Linux-Kernel/archive/refs/tags/${TAG}.tar.gz"
    mkdir src
    tar xzf src.tar.gz -C src --strip-components=1
    rm src.tar.gz
fi
cd src

echo "==> configuring from the running kernel's /proc/config.gz"
zcat /proc/config.gz > .config
scripts/config --module CONFIG_CAN_VCAN --module CONFIG_CAN_VXCAN
make -s olddefconfig
grep -E "^CONFIG_CAN(_DEV|_VCAN|_VXCAN)=" .config

echo "==> make modules_prepare (-j${JOBS})"
make -s -j"${JOBS}" modules_prepare

echo "==> building drivers/net/can modules"
# No Module.symvers is available for this kernel (see header), so tell modpost to
# report symbols it cannot find CRCs for as warnings instead of errors.
make -s -j"${JOBS}" KBUILD_MODPOST_WARN=1 M=drivers/net/can modules

cp drivers/net/can/vcan.ko drivers/net/can/vxcan.ko "$OUT"/
echo "==> recorded symbol versions in vcan.ko (symbols imported from can-dev):"
modprobe --dump-modversions "$OUT/vcan.ko" || true
echo "$TAG" > "$OUT/source-tag"
echo "==> built:"
modinfo "$OUT/vcan.ko" | grep -E "^(filename|vermagic|depends|srcversion):"

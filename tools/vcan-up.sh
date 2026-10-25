#!/usr/bin/env bash
# Create and bring up the virtual CAN interfaces used by the project.
#
# vcan0: the bus both ECUs sit on in the plain topology.
# vcan1: the second bus segment used when the Python fault-injection gateway
#        (Phase 3) is inserted between the two ECUs.
#
# Runs inside the dev container, which must have CAP_NET_ADMIN
# (docker run --cap-add=NET_ADMIN). The vcan kernel module has to be available
# in the kernel the container runs on; on Docker Desktop/WSL2 see tools/wsl2/.
set -euo pipefail

SUDO=""
if [[ "$(id -u)" -ne 0 ]]; then
    SUDO="sudo -n"
fi

IFACES=("$@")
if [[ ${#IFACES[@]} -eq 0 ]]; then
    IFACES=(vcan0 vcan1)
fi

for ifc in "${IFACES[@]}"; do
    if ! ip link show "$ifc" >/dev/null 2>&1; then
        if ! $SUDO ip link add dev "$ifc" type vcan 2>/tmp/vcan-up.err; then
            echo "error: cannot create $ifc: $(cat /tmp/vcan-up.err)" >&2
            echo "hint: the container needs --cap-add=NET_ADMIN and the kernel needs the vcan" >&2
            echo "      module (Docker Desktop on Windows: run tools/wsl2/install-vcan-module.ps1)" >&2
            exit 1
        fi
    fi
    $SUDO ip link set up "$ifc"
done

ip -brief link show type vcan

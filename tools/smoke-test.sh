#!/usr/bin/env bash
# Smoke test: run the native_sim firmware against a vcan interface, prove with
# candump that frames with the expected identifier appear, and verify every
# captured frame (decode, CRC, rolling counter) with the independent Python
# implementation in can/.
#
#   tools/smoke-test.sh [path/to/zephyr.exe] [interface] [frames] [id-hex]
#
# Exit status is 0 only if the requested number of frames were captured within
# the timeout and all of them pass verification. Runs inside the dev container.
set -euo pipefail

EXE="${1:-build/native_sim_sensor/zephyr/zephyr.exe}"
IFACE="${2:-vcan0}"
WANT="${3:-5}"
ID_HEX="${4:-200}"        # SENSOR_HEARTBEAT
# candump prints standard identifiers as three upper-case hex digits; normalise
# the argument the same way so the count below cannot miss them.
ID_HEX="$(printf '%03X' "0x${ID_HEX}")"
TIMEOUT_S=10
LOG="$(mktemp)"

tools/vcan-up.sh "$IFACE" >/dev/null

"$EXE" --can-if="$IFACE" >"$LOG.app" 2>&1 &
APP_PID=$!
trap 'kill $APP_PID 2>/dev/null || true; wait $APP_PID 2>/dev/null || true' EXIT

# -n N: exit after N frames; the ID filter keeps unrelated traffic out of the count.
if timeout "$TIMEOUT_S" candump -n "$WANT" -T 5000 "$IFACE,${ID_HEX}:7FF" | tee "$LOG"; then
    :
fi

GOT="$(grep -c " ${ID_HEX} " "$LOG" || true)"
echo "captured $GOT/$WANT frames with id 0x${ID_HEX} on $IFACE"
echo "--- independent verification (cantools decode + can/e2e_ref.py CRC and counter check)"
VERIFY_OK=1
python3 can/verify_candump.py --min-protected "$WANT" "$LOG" || VERIFY_OK=0
echo "--- firmware log"
cat "$LOG.app"
rm -f "$LOG" "$LOG.app"

[[ "$GOT" -ge "$WANT" && "$VERIFY_OK" -eq 1 ]]

#!/usr/bin/env bash
# Phase 0 smoke test: run the native_sim firmware against vcan0 and prove with
# candump that frames with the expected identifier appear on the interface.
#
#   tools/smoke-test.sh [path/to/zephyr.exe] [interface] [frames]
#
# Exit status is 0 only if the requested number of frames with ID 0x100 were
# captured within the timeout. Runs inside the dev container.
set -euo pipefail

EXE="${1:-build/native_sim/zephyr/zephyr.exe}"
IFACE="${2:-vcan0}"
WANT="${3:-3}"
TIMEOUT_S=10
LOG="$(mktemp)"

tools/vcan-up.sh "$IFACE" >/dev/null

"$EXE" --can-if="$IFACE" >"$LOG.app" 2>&1 &
APP_PID=$!
trap 'kill $APP_PID 2>/dev/null || true; wait $APP_PID 2>/dev/null || true' EXIT

# -n N: exit after N frames; the ID filter keeps unrelated traffic out of the count.
if timeout "$TIMEOUT_S" candump -n "$WANT" -T 5000 "$IFACE,100:7FF" | tee "$LOG"; then
    :
fi

GOT="$(grep -c " 100 " "$LOG" || true)"
echo "captured $GOT/$WANT frames with id 0x100 on $IFACE"
echo "--- firmware log"
cat "$LOG.app"
rm -f "$LOG" "$LOG.app"

[[ "$GOT" -ge "$WANT" ]]

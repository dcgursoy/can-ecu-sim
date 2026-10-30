#!/usr/bin/env bash
# Record the demo (tools/demo.py) under a pseudo-terminal and render it to a GIF.
#
#   tools/record-demo.sh [--out DIR] [--publish] [demo.py options...]
#
# Writes DIR/demo.cast (asciicast v2, what asciinema records) and DIR/demo.gif
# (rendered by agg), default DIR build/demo; --publish also copies the GIF to
# docs/figures/demo.gif, the one the README shows. The demo's capture and ECU
# logs land in DIR as well. Runs inside the dev container (pty, vcan, agg).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

OUT="build/demo"
PUBLISH=0
DEMO_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out) OUT="$2"; shift ;;
        --publish) PUBLISH=1 ;;
        *) DEMO_ARGS+=("$1") ;;
    esac
    shift
done
mkdir -p "$OUT"

python3 tools/record_cast.py "$OUT/demo.cast" --cols 110 --rows 36 -- \
    python3 tools/demo.py --out "$OUT" --color ${DEMO_ARGS[@]+"${DEMO_ARGS[@]}"}
# The font is the one the image installs (fonts-dejavu-core); 2 s is the longest pause kept.
agg --font-family "DejaVu Sans Mono" --font-size 15 --theme monokai --idle-time-limit 2 \
    "$OUT/demo.cast" "$OUT/demo.gif"
ls -l "$OUT/demo.gif"
if [[ "$PUBLISH" -eq 1 ]]; then
    mkdir -p docs/figures
    cp "$OUT/demo.gif" docs/figures/demo.gif
    echo "published docs/figures/demo.gif"
fi

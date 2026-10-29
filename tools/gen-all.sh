#!/usr/bin/env bash
# Regenerate every file derived from can/network.dbc, or verify them (--check).
#
#   tools/gen-all.sh           # rewrite the generated headers
#   tools/gen-all.sh --check   # exit 1 if any committed generated file is stale (CI)
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
python3 can/gen_codec.py "$@"
python3 can/gen_test_vectors.py "$@"
python3 can/gen_rust.py "$@"

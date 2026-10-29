#!/usr/bin/env bash
# Build, lint and test the Rust sensor ECU (rust/), inside the dev container.
#
#   tools/build-rust.sh            # everything below
#   tools/build-rust.sh --quick    # tests and the release binary only (no lint, no cross-compile)
#
# Steps, each one fatal:
#   1. rustfmt (check mode) and clippy with every warning an error, all targets;
#   2. the unit tests of both crates and the conformance tests of sensor-core
#      against the cantools / e2e_ref.py vectors (rust/sensor-core/tests/);
#   3. the release build of sensor-vcan, the executable the system suite runs
#      with --sensor-exe rust/target/release/sensor-vcan;
#   4. sensor-core cross-compiled for thumbv7em-none-eabihf (Cortex-M4F, the
#      Nucleo-F446RE's core): the proof that the crate is no_std and free of
#      anything the embedded target lacks (no std, no allocator, no float
#      library beyond libm). The rlib is built, not linked: a firmware image
#      needs a board crate the project does not have.
#
# Cargo.lock is committed; --locked makes a build fail instead of silently
# resolving different dependency versions than the ones that were tested.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT/rust"

QUICK=0
if [[ "${1:-}" == "--quick" ]]; then
    QUICK=1
fi

# The registry cache lives in CARGO_HOME, which the container image provides;
# the compiled artifacts land in rust/target on the bind mount and persist.
export CARGO_TERM_COLOR="${CARGO_TERM_COLOR:-never}"
export CARGO_NET_RETRY=5

echo "== toolchain"
rustc --version
cargo --version

if [[ "$QUICK" -eq 0 ]]; then
    echo "== rustfmt"
    cargo fmt --all -- --check
    echo "== clippy, every warning an error"
    cargo clippy --workspace --all-targets --locked -- -D warnings
fi

echo "== tests"
cargo test --workspace --locked

echo "== release build, sensor-vcan"
cargo build --release --locked -p sensor-vcan
ls -l target/release/sensor-vcan

if [[ "$QUICK" -eq 0 ]]; then
    echo "== cross-compile sensor-core for thumbv7em-none-eabihf"
    cargo build --release --locked -p sensor-core --target thumbv7em-none-eabihf
    ls -l target/thumbv7em-none-eabihf/release/libsensor_core.rlib
fi
echo "rust: OK"

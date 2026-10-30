#!/usr/bin/env bash
# Print the versions of every tool in the dev image (kept in docs/toolchain-versions.txt).
set -uo pipefail

ver_field() { sed -n "s/^$1 *= *//p" "$ZEPHYR_BASE/VERSION"; }
ZEPHYR_VERSION="$(ver_field VERSION_MAJOR).$(ver_field VERSION_MINOR).$(ver_field PATCHLEVEL)"

echo "image:        $(. /etc/os-release && echo "$PRETTY_NAME") ($(uname -m))"
echo "zephyr:       v${ZEPHYR_VERSION} commit $(git -C "$ZEPHYR_BASE" rev-parse --short HEAD) (ZEPHYR_BASE=$ZEPHYR_BASE)"
echo "zephyr sdk:   $(cat "$ZEPHYR_SDK_INSTALL_DIR/sdk_version") at $ZEPHYR_SDK_INSTALL_DIR"
echo "arm gcc:      $("$ZEPHYR_SDK_INSTALL_DIR"/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc --version | head -1)"
echo "host gcc:     $(gcc --version | head -1)"
echo "cmake:        $(cmake --version | head -1)"
echo "ninja:        $(ninja --version)"
echo "dtc:          $(dtc --version)"
echo "python:       $(python3 --version)"
echo "west:         $(west --version)"
echo "python-can:   $(python3 -c 'import can; print(can.__version__)')"
echo "cantools:     $(python3 -c 'import cantools; print(cantools.__version__)')"
echo "pytest:       $(pytest --version 2>&1 | head -1)"
echo "rustc:        $(rustc --version)"
echo "cargo:        $(cargo --version)"
echo "rust targets: $(rustup target list --installed | tr '\n' ' ')"
echo "can-utils:    $(dpkg-query -W -f='${Version}' can-utils)"
echo "clang-tidy:   $(clang-tidy --version | grep -i version | head -1 | sed 's/^ *//')"
echo "cppcheck:     $(cppcheck --version)"
echo "gcovr:        $(gcovr --version | head -1)"
echo "lcov:         $(lcov --version | head -1)"
echo "matplotlib:   $(python3 -c 'import matplotlib; print(matplotlib.__version__)')"
echo "agg:          $(agg --version)"

# Dev container for can-ecu-sim.
#
# One image serves three jobs: interactive development (VS Code Dev Containers),
# local command-line builds (tools/dev.sh) and CI (.github/workflows/ci.yml), so
# every build and test runs with the same toolchain regardless of the host OS.
#
# Contents: Zephyr SDK (arm-zephyr-eabi toolchain), a west workspace with Zephyr
# pinned by firmware/west.yml, can-utils for SocketCAN, Python host tools
# (python-can, cantools, pytest), Rust with the thumbv7em-none-eabihf target, and
# the static analysers used in CI (clang-tidy, cppcheck, gcovr).
#
# The image is x86_64 only (Zephyr SDK tarball, gcc-multilib); the wrappers pass
# --platform linux/amd64 so the choice is explicit on every host.
FROM ubuntu:24.04

ARG ZEPHYR_SDK_VERSION=0.17.4
ARG RUST_VERSION=1.89.0
ARG USERNAME=dev
# On Linux hosts tools/dev.sh --build passes the host uid/gid so the bind-mounted
# checkout is writable; Docker Desktop (Windows/macOS) and CI do not need this.
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# --- System packages -----------------------------------------------------------
# gcc-multilib/g++-multilib: native_sim is a 32-bit target by default, which keeps
# the simulated firmware closer to the Cortex-M4 data model (32-bit int/pointer).
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl wget xz-utils file git sudo \
        build-essential gcc-multilib g++-multilib \
        cmake ninja-build gperf ccache device-tree-compiler \
        python3 python3-dev python3-pip python3-venv \
        can-utils iproute2 kmod iputils-ping procps socat jq less nano \
        clang-tidy cppcheck lcov \
    && rm -rf /var/lib/apt/lists/*

# --- Non-root user with passwordless sudo (needed for `ip link add ... vcan`) ----
RUN userdel -r ubuntu 2>/dev/null || true \
    && groupadd -g ${USER_GID} ${USERNAME} \
    && useradd -m -u ${USER_UID} -g ${USER_GID} -s /bin/bash ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}
LABEL can-ecu-sim.uid=${USER_UID}

# Git must trust repositories owned by another uid: the bind-mounted checkout
# (owned by the host user) and, when CI runs the container as root, the
# dev-owned west workspace below. System scope covers every user.
RUN git config --system --add safe.directory '*'

# --- Zephyr SDK: minimal bundle + the one cross toolchain this project uses ------
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION} \
    ZEPHYR_TOOLCHAIN_VARIANT=zephyr
RUN cd /opt \
    && wget -q "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" \
    && tar xf "zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" \
    && rm "zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" \
    && "zephyr-sdk-${ZEPHYR_SDK_VERSION}/setup.sh" -t arm-zephyr-eabi \
    && chown -R ${USERNAME}:${USERNAME} "/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}"

# --- Python virtualenv with west + host tooling -----------------------------------
ENV VIRTUAL_ENV=/opt/venv
ENV PATH=${VIRTUAL_ENV}/bin:${PATH}
COPY requirements-dev.txt /tmp/requirements-dev.txt
RUN python3 -m venv ${VIRTUAL_ENV} \
    && pip install --no-cache-dir --upgrade pip \
    && pip install --no-cache-dir -r /tmp/requirements-dev.txt \
    && chown -R ${USERNAME}:${USERNAME} ${VIRTUAL_ENV}

# --- Zephyr west workspace pinned by firmware/west.yml ----------------------------
# The firmware is built as a Zephyr "freestanding" application against this
# workspace (ZEPHYR_BASE), so the repository checkout can live anywhere. Because
# the manifest is copied in here, a change to firmware/west.yml only takes effect
# after the image is rebuilt; firmware/app/CMakeLists.txt checks for that.
# hal_stm32 ships every STM32 family (about 1 GB of sources); only the F4 family
# used by nucleo_f446re is kept (6.69 GB -> 5.73 GB image), so the hal_stm32
# working tree in the image is intentionally partial.
ENV ZEPHYR_WORKSPACE=/opt/zephyrproject
ENV ZEPHYR_BASE=${ZEPHYR_WORKSPACE}/zephyr
COPY firmware/west.yml ${ZEPHYR_WORKSPACE}/manifest/west.yml
RUN chown -R ${USERNAME}:${USERNAME} ${ZEPHYR_WORKSPACE}
USER ${USERNAME}
RUN cd ${ZEPHYR_WORKSPACE} \
    && west init -l manifest \
    && west update --narrow -o=--depth=1 \
    && find ${ZEPHYR_WORKSPACE}/modules/hal/stm32/stm32cube -mindepth 1 -maxdepth 1 \
            -type d -name "stm32*" ! -name "stm32f4xx" -exec rm -rf {} + \
    && west zephyr-export \
    && pip install --no-cache-dir \
        -r ${ZEPHYR_BASE}/scripts/requirements-base.txt \
        -r ${ZEPHYR_BASE}/scripts/requirements-build-test.txt \
        -r ${ZEPHYR_BASE}/scripts/requirements-run-test.txt

# --- Rust (stable, pinned) with the Cortex-M4F target for the Phase 7 port --------
ENV RUSTUP_HOME=/home/${USERNAME}/.rustup \
    CARGO_HOME=/home/${USERNAME}/.cargo
ENV PATH=${CARGO_HOME}/bin:${PATH}
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path \
        --profile minimal --default-toolchain ${RUST_VERSION} \
        --target thumbv7em-none-eabihf \
        --component clippy,rustfmt

WORKDIR /workspaces/can-ecu-sim
CMD ["/bin/bash"]

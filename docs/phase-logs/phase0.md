# Phase 0 verification log

Environment: Windows 11 host, Docker Desktop 4.88.1 (Docker Engine 29.7.2) on
the WSL2 backend, kernel `6.6.87.2-microsoft-standard-WSL2`, 8 cores, 16 GB.
Everything below ran inside the dev container image built from
`.devcontainer/Dockerfile` (`docker images` reports 5.71 GB; the first version
without the STM32 HAL pruning was 6.69 GB). Tool versions:
`docs/toolchain-versions.txt`.

## 1. vcan on the WSL2 kernel

The stock kernel has `CONFIG_CAN=m`, `CONFIG_CAN_RAW=m`, `CONFIG_CAN_DEV=m`,
`CONFIG_CAN_GW=m` but `# CONFIG_CAN_VCAN is not set`, so
`ip link add dev vcan0 type vcan` fails with `Error: Unknown device type.`

Command:

```bash
tools/wsl2/install-vcan-module.sh
```

Result (excerpt):

```
==> configuring from the running kernel's /proc/config.gz
CONFIG_CAN_DEV=m
CONFIG_CAN_VCAN=m
CONFIG_CAN_VXCAN=m
==> recorded symbol versions in vcan.ko (symbols imported from can-dev):
0xd4380d17	can_dropped_invalid_skb
==> built:
depends:        can-dev
vermagic:       6.6.87.2-microsoft-standard-WSL2 SMP preempt mod_unload modversions
==> installing into the VM's /lib/modules/6.6.87.2-microsoft-standard-WSL2 and loading
vxcan 12288 0 - Live 0xffffffffc05fb000 (O)
vcan 12288 0 - Live 0xffffffffc05d1000 (O)
can_dev 36864 2 vxcan,vcan, Live 0xffffffffc05c2000
taint: 4096
```

Taint value 4096 is bit 12, `O` (out-of-tree module), the expected flag for any
module not built with the kernel. It is not the `F` (forced load) flag.

**Persistence test.** `docker desktop stop` followed by `docker desktop start`
kept the VM (its `/proc/uptime` continued from 6730 s) and the module stayed
loaded. `wsl --shutdown` followed by `docker desktop start` rebooted the VM
(uptime 9.85 s afterwards); `vcan.ko` was gone from `/lib/modules` and
`ip link add type vcan` failed again. The module directory's writable layer is
RAM-backed. After that reboot, the first `tools/dev.sh` call detected the
missing module and reinstalled it from the cached build:

```
$ time tools/dev.sh bash -c 'tools/vcan-up.sh && echo "container sees vcan"'
==> verifying from an unprivileged NET_ADMIN container
3: vcan0: <NOARP,UP,LOWER_UP> mtu 72 qdisc noqueue state UNKNOWN qlen 1000
==> done. vcan is available to containers started with --cap-add=NET_ADMIN.
vcan0            UNKNOWN        <NOARP,UP,LOWER_UP>
vcan1            UNKNOWN        <NOARP,UP,LOWER_UP>
container sees vcan
real    0m7.728s
```

## 2. Firmware builds

```bash
tools/dev.sh west build -p always -b native_sim -s firmware/app -d build/native_sim
tools/dev.sh west build -p always -b nucleo_f446re -s firmware/app -d build/nucleo_f446re
```

Both succeed, also when run as root the way CI does
(`CAN_ECU_SIM_DOCKER_ARGS="--user root"`). The Nucleo link map:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:       34784 B       512 KB      6.63%
             RAM:        7360 B       128 KB      5.62%
```

Configuration facts checked in the generated `.config` files: both targets run
`CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000`; the Nucleo has `CONFIG_PICOLIBC=y`,
`CONFIG_GLIBCXX_LIBCPP=y`, `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0`,
`CONFIG_HEAP_MEM_POOL_SIZE=0` and deferred logging; `native_sim` uses immediate
logging. `arm-zephyr-eabi-nm` on the Nucleo ELF finds none of `malloc`, `calloc`,
`realloc`, `free`, `operator new` or picolibc's `malloc_prepare`.

The first attempt failed with `fatal error: array: No such file or directory`:
Zephyr's default minimal C++ library ships no STL headers. Fixed with
`CONFIG_REQUIRES_FULL_LIBCPP=y` (header-only use of libstdc++; exceptions and
RTTI remain off).

The CMake guard against a stale image was exercised by appending a comment to
`firmware/west.yml` and configuring: the build stops with "firmware/west.yml
differs from the manifest baked into the dev image; rebuild the image with:
tools/dev.sh --build".

## 3. Smoke test: candump sees the frame

```bash
tools/dev.sh tools/smoke-test.sh build/native_sim/zephyr/zephyr.exe
```

```
  vcan0  100   [8]  48 45 4C 4C 4F 00 00 00
  vcan0  100   [8]  48 45 4C 4C 4F 00 00 01
  vcan0  100   [8]  48 45 4C 4C 4F 00 00 02
captured 3/3 frames with id 0x100 on vcan0
--- firmware log
*** Booting Zephyr OS build 75f67d766726 ***
[00:00:00.000,000] <inf> app: CAN device can started, sending 0x100 every 1000 ms
[00:00:00.000,000] <inf> app: sent id=0x100 counter=0
[00:00:01.001,000] <inf> app: sent id=0x100 counter=1
[00:00:02.000,900] <inf> app: sent id=0x100 counter=2
```

`48 45 4C 4C 4F` is "HELLO"; the last three bytes are the frame counter.

**Period as seen on the bus** (host-side timestamps from `candump -td vcan0,100:7FF`,
measured on a simulated bus on a desktop OS, not real-time hardware):

```
 (000.999900)  (001.000551)  (000.999723)  (001.000109)  (001.000141)  (001.000147)  (001.000091)
```

Seven consecutive deltas between 0.99972 s and 1.00055 s. The first version of
the loop used `k_sleep(K_MSEC(1000))` after the send and showed a deterministic
1.020 s period: `native_sim` ticked at 100 Hz, a relative timeout is rounded up
by one tick, and the send time was added on every iteration. The fix was a
`k_timer` with absolute expiries plus a 10 kHz tick (`prj.conf`). The remaining
sub-millisecond spread is host scheduling of the process and of `candump`.

**Fatal path.** With a non-existent interface the process must not idle forever:

```
$ timeout 10 build/native_sim/zephyr/zephyr.exe --can-if=nosuch; echo exit=$?
<err> can_native_linux: Cannot open nosuch (-19)
<err> app: CAN device is not ready (-19)
<err> os: >>> ZEPHYR FATAL ERROR 4: Kernel panic on CPU 0
Exiting due to fatal error
exit=1          (after 8 ms)
```

## 4. Dev container definition

`npx @devcontainers/cli up` built and started the container from
`.devcontainer/devcontainer.json`: user `dev`, `vcan0`/`vcan1` created by the
`postStartCommand`, `ZEPHYR_BASE` set, and a `west build` for `native_sim`
succeeded inside it.

## 5. Review

Before the commit an independent adversarial review (four lenses, every finding
checked by two verifiers) produced 36 confirmed findings; all were fixed. The
ones that would have broken CI or misled a reader: shell scripts staged without
the executable bit (invisible on a Windows bind mount), `--user root` in CI
tripping git's "dubious ownership" check on the Zephyr tree, the PowerShell
wrapper binding west's `-b`/`-p`/`-d` flags to its own parameters, the false
claim that the vcan module survives VM reboots, `can_send()` with a NULL callback
waiting unbounded for completion, `return 1` from `main()` being ignored by
Zephyr, picolibc's default malloc arena claiming all free SRAM on the Nucleo,
immediate-mode logging in CAN callbacks on hardware, and the 1.020 s period.

## 6. CI

`.github/workflows/ci.yml` runs the same commands on `ubuntu-24.04` through
`tools/dev.sh` after `sudo modprobe vcan` on the runner. It has not run yet
because the repository has no GitHub remote at the time of writing; the first
green run will be recorded here.

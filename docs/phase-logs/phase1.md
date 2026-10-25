# Phase 1 verification log

Same environment as Phase 0 (Docker Desktop 4.88.1 on WSL2, kernel 6.6.87.2).
The dev image was rebuilt with GoogleTest (`libgtest-dev`, `libgmock-dev`).
Every command below ran through `tools/dev.sh` inside that image, after the
adversarial review of this phase (35 findings, all fixed; see section 6).

## 1. Generated code

```
$ tools/gen-all.sh
wrote firmware/lib/protocol/include/protocol/messages.hpp (9 messages, 6 enums)
wrote firmware/lib/protocol/tests/generated/test_vectors.hpp
$ tools/gen-all.sh --check
firmware/lib/protocol/include/protocol/messages.hpp is up to date
firmware/lib/protocol/tests/generated/test_vectors.hpp is up to date
```

## 2. Polynomial choice

```
$ python3 can/crc_hd.py
data length 72 bits, codeword 80 bits (data + 8 CRC bits)
poly 0x2F AUTOSAR CRC8H2F (E2E P02)            HD=4   undetected w=1: 0/80  w=2: 0/3160  w=3: 0/82160  w=4: 12629/1581580
poly 0x1D SAE J1850 / AUTOSAR CRC8 (E2E P01)   HD=3   undetected w=1: 0/80  w=2: 0/3160  w=3: 362/82160  w=4: 6152/1581580
```

The first draft of the scheme used 0x1D; this measurement switched it to 0x2F
before anything was committed. The first version of the script counted only
error patterns inside the 72 data bits; the review pointed out that a code's
Hamming distance is a property of the codeword including the check bits, so
the 8 CRC-bit positions were added. The conclusion did not change. Both the
Python reference and the C++ header reproduce the seven CRC8H2F check values
of the AUTOSAR CRC library specification (tests
`test_crc8_matches_autosar_check_values` and `Crc8.AutosarCheckValues`).

## 3. Unit tests

```
$ python3 -m pytest -q can/
8 passed in 0.10s

$ tools/run-host-tests.sh build/host-tests
100% tests passed, 0 tests failed out of 34
lines: 96.7% (667 out of 690)
functions: 96.3% (103 out of 107)
branches: 77.7% (265 out of 341)
```

Coverage is of the library headers only (`firmware/lib/protocol/include/`);
GoogleTest and the test sources are excluded. The build uses
`-fkeep-inline-functions`, so an inline function that no test calls is counted
as uncovered instead of vanishing from both numerator and denominator (without
that flag the same tests report 99.2 % lines and 100 % functions, which is the
flattering number, not the true one). What remains uncovered:

- `crc8::make_table()` and the `std::array` overload of `crc8::compute()`,
  evaluated only at compile time (the table is a `constexpr` object, the
  overload is used in a `static_assert`);
- the `protect<ControllerCommand>()` instantiation, likewise used only inside a
  `static_assert`;
- the `return "UNKNOWN"` fall-through of `to_string(CanState)`: a 2-bit field
  whose four values are all named, so no raw value reaches it (the other five
  enums' fall-throughs are reached and tested over their whole raw range);
- compiler-generated members of two message structs that no test needs.

The tests compile with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion -Wdouble-promotion -Wold-style-cast -Werror`.

Defects the tests and the review caught during development, kept here because
they are the kind of thing an interviewer asks about:

- The first generator emitted free `unpack(const Frame&)` functions. Every
  8-byte message has the same `Frame` type, so the overloads collided and the
  compiler silently picked the first one for every message. The codec moved to
  per-type `Msg::from_frame()` / `msg.to_frame()`.
- `FieldInfo` held raw bounds as `int32_t`; the 32-bit unsigned `uptime` and
  `payload` signals (max 4,294,967,295) failed to compile under `-Wnarrowing`.
  Now `int64_t`, and the generator refuses scaled 32-bit unsigned signals,
  whose physical accessors would convert through `int32_t`.
- `phys_to_raw()` was undefined behaviour for NaN (both saturation guards are
  false for NaN, so the float-to-int conversion ran on it; found with
  `-fsanitize=float-cast-overflow`), and its `+0.5 then truncate` rounding
  rounded twice. It now maps NaN to the raw minimum and rounds in two exact
  steps; both have tests.
- The counter tracker moved its reference on an out-of-sequence frame, so one
  late frame produced a second, phantom "skipped" fault on the next good frame.
  It now keeps the last accepted counter (tests in both implementations).
- The two ECUs shared one echo-response identifier, which vcan tolerates and a
  real bus does not. Each ECU now has its own.

## 4. Firmware builds

```
$ west build -p always -b native_sim -s firmware/app -d build/native_sim      (clean)
$ west build -p always -b nucleo_f446re -s firmware/app -d build/nucleo_f446re
           FLASH:       35532 B       512 KB      6.78%
             RAM:        7360 B       128 KB      5.62%
```

The Phase 1 image is 748 bytes of flash larger than Phase 0's (35,532 vs
34,784 B). That delta also contains the new heartbeat assembly, the stale
TX-completion handling and longer log strings in `main.cpp`, so it is an upper
bound for the library: `arm-zephyr-eabi-nm --size-sort` attributes 256 B to the
compile-time CRC table (`proto::crc8::kTable`) and 52 B to
`proto::codec::put<8>`; the rest of the codec and E2E code is inlined into the
caller.

## 5. Live-bus verification

```
$ tools/smoke-test.sh build/native_sim/zephyr/zephyr.exe
  vcan0  200   [8]  69 10 00 00 00 00 00 00
  vcan0  200   [8]  56 11 64 00 00 00 00 00
  vcan0  200   [8]  17 12 C8 00 00 00 00 00
  vcan0  200   [8]  FF 13 2C 01 00 00 00 00
  vcan0  200   [8]  CC 14 90 01 00 00 00 00
captured 5/5 frames with id 0x200 on vcan0
--- independent verification (cantools decode + can/e2e_ref.py CRC and counter check)
SENSOR_HEARTBEAT         0x200 6910000000000000  crc=105, counter=0, node_state=RUNNING, uptime=0, can_state=ERROR_ACTIVE, bus_off_count=0, error_flags=0
SENSOR_HEARTBEAT         0x200 5611640000000000  crc=86, counter=1, node_state=RUNNING, uptime=100, can_state=ERROR_ACTIVE, bus_off_count=0, error_flags=0
SENSOR_HEARTBEAT         0x200 1712c80000000000  crc=23, counter=2, node_state=RUNNING, uptime=200, can_state=ERROR_ACTIVE, bus_off_count=0, error_flags=0
SENSOR_HEARTBEAT         0x200 ff132c0100000000  crc=255, counter=3, node_state=RUNNING, uptime=300, can_state=ERROR_ACTIVE, bus_off_count=0, error_flags=0
SENSOR_HEARTBEAT         0x200 cc14900100000000  crc=204, counter=4, node_state=RUNNING, uptime=400, can_state=ERROR_ACTIVE, bus_off_count=0, error_flags=0
verified 5 frames (5 protected, CRC-checked): 0 wrong DLC, 0 bad CRC, 0 counter faults, 0 unknown ids, 0 unparsed lines -> OK
```

Byte 0 is the CRC; byte 1 holds `node_state` in the high nibble (1 = RUNNING)
and the rolling counter in the low nibble, hence 0x10, 0x11, 0x12, ...; bytes
2-5 are the little-endian uptime in milliseconds (0x64 = 100); byte 6 the CAN
state (error-active = 0) and bus-off count; byte 7 the error flags. The frames
were produced by the C++ firmware running under Zephyr and verified by the
Python reference, which shares no code with it. The verifier insists on at
least as many CRC-checked protected frames as the test asked for, so a run in
which nothing was actually verified cannot pass.

## 6. Review

An independent adversarial review (four lenses, every finding checked by two
verifiers) produced 35 confirmed findings; all were fixed before the commit.
The substantive ones are listed in section 3 above; the rest were accuracy
problems in the documentation (the polynomial attributed to the wrong E2E
profile, a claimed independence of implementations that the code comments
themselves contradicted, `native_sim` described as cross-compiled, stale
toolchain and CI descriptions) and robustness gaps in the tooling (the candump
verifier crashed on a wrong DLC and silently skipped timestamped lines, the
smoke test miscounted lower-case identifier arguments, the coverage artifact
lacked its JavaScript file).

// Umbrella header for the platform-independent protocol library.
//
// The library has no dependency on Zephyr or on any OS: it is plain C++17,
// header-only, constexpr where possible and never allocates. The same headers
// are compiled into the firmware (native_sim and nucleo_f446re) and into the
// host unit tests.
#pragma once

#include "protocol/codec_util.hpp"
#include "protocol/crc8.hpp"
#include "protocol/e2e.hpp"
#include "protocol/messages.hpp"

// Hardware watchdog behind the Zephyr watchdog API. nucleo_f446re: the IWDG
// (LSI clocked, cannot be stopped once started). native_sim: a counter-based
// watchdog over the simulator's counter device. Both are reached through the
// devicetree alias watchdog0, so the supervisor has one code path.
#pragma once

#include <zephyr/device.h>

#include <cstdint>

namespace hal {

class Watchdog {
public:
    /// Install a single timeout that resets the SoC and start the watchdog.
    int init(std::uint32_t timeout_ms);
    int feed();
    bool available() const { return dev_ != nullptr && channel_ >= 0; }
    std::uint32_t timeout_ms() const { return timeout_ms_; }
    std::uint32_t feeds() const { return feeds_; }

private:
    const device* dev_ = nullptr;
    int channel_ = -1;
    std::uint32_t timeout_ms_ = 0;
    std::uint32_t feeds_ = 0;
};

}  // namespace hal

#include "hal/watchdog.hpp"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

#include <cerrno>

// The STM32 IWDG driver installs its own timeout at boot unless told not to,
// and then rejects ours with -ENOMEM; that would be a 100 ms reset loop on the
// board. Make the misconfiguration a build error instead.
#if defined(CONFIG_IWDG_STM32) && !defined(CONFIG_WDT_DISABLE_AT_BOOT)
#error "set CONFIG_WDT_DISABLE_AT_BOOT=y (boards/nucleo_f446re.conf): the IWDG driver would arm itself at boot"
#endif

LOG_MODULE_REGISTER(hal_wdt, LOG_LEVEL_INF);

namespace hal {

int Watchdog::init(std::uint32_t timeout_ms)
{
    timeout_ms_ = timeout_ms;
    dev_ = DEVICE_DT_GET_OR_NULL(DT_ALIAS(watchdog0));
    if (dev_ == nullptr || !device_is_ready(dev_)) {
        dev_ = nullptr;
        return -ENODEV;
    }

    wdt_timeout_cfg cfg{};
    cfg.window.min = 0;
    cfg.window.max = timeout_ms;
    cfg.callback = nullptr;  // no warning callback: expiry resets the SoC
    cfg.flags = WDT_FLAG_RESET_SOC;
    channel_ = wdt_install_timeout(dev_, &cfg);
    if (channel_ < 0) {
        LOG_ERR("wdt_install_timeout failed (%d)", channel_);
        return channel_;
    }
    const int err = wdt_setup(dev_, 0);
    if (err != 0) {
        LOG_ERR("wdt_setup failed (%d)", err);
        channel_ = -1;
        return err;
    }
    LOG_INF("watchdog %s armed, %u ms", dev_->name, timeout_ms);
    return 0;
}

int Watchdog::feed()
{
    if (!available()) {
        return -ENODEV;
    }
    const int err = wdt_feed(dev_, channel_);
    if (err == 0) {
        ++feeds_;
    }
    return err;
}

}  // namespace hal

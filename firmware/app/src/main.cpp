// Entry point: bring up the application (HAL, queues, threads) and hand over.
// Zephyr ignores main()'s return value, so a failed start stops the system
// explicitly: on native_sim k_panic() exits the process with status 1, on
// Cortex-M it halts in the fatal-error handler where the watchdog, if it was
// already armed, resets the part.
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "app/ecu.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main()
{
    const int err = app::start();
    if (err != 0) {
        LOG_ERR("application start failed (%d)", err);
        k_panic();
        CODE_UNREACHABLE;
    }
    LOG_INF("%s ECU running", app::role::kName);
    return 0;
}

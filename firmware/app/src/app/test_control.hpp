// TEST_CONTROL message handler: the software fault-injection entry point.
// Compiled only with CONFIG_APP_TEST_HOOKS; without it the RX thread counts the
// message as unknown and nothing on the bus can influence the firmware's
// behaviour beyond the normal protocol.
#pragma once

#include <zephyr/drivers/can.h>

#include <cstdint>

namespace app::test_control {

void handle(const can_frame& frame, std::int64_t now_ms);

}  // namespace app::test_control

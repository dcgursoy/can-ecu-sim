#include <gtest/gtest.h>

#include "ecu/liveness.hpp"

namespace {

TEST(Liveness, NothingIsAliveBeforeTheFirstCheckIn)
{
    ecu::Liveness<3> l(200);
    EXPECT_EQ(l.stale_mask(0), 0x7U);
    EXPECT_FALSE(l.all_alive(0));
    EXPECT_EQ(l.window_ms(), 200U);
}

TEST(Liveness, StaleThreadsShowUpInTheMask)
{
    ecu::Liveness<3> l(200);
    l.check_in(0, 1000);
    l.check_in(1, 1000);
    l.check_in(2, 1000);
    EXPECT_TRUE(l.all_alive(1200));
    EXPECT_FALSE(l.all_alive(1201));
    l.check_in(0, 1300);
    l.check_in(2, 1300);
    EXPECT_EQ(l.stale_mask(1301), 0x2U) << "only thread 1 missed its check-in";
    l.check_in(7, 1300);  // out of range is ignored
    EXPECT_EQ(l.stale_mask(1301), 0x2U);
}

}  // namespace

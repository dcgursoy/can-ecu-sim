#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "ecu/timeout_monitor.hpp"

namespace {

constexpr std::array<ecu::MonitoredMessage, 2> kCfg{{
    {0x100, 35, "SENSOR_DATA"},
    {0x200, 350, "SENSOR_HEARTBEAT"},
}};

struct Recorder {
    std::vector<std::uint32_t> lost;
    void operator()(const ecu::TimeoutMonitor<2>::Entry& e) { lost.push_back(e.cfg.id); }
};

TEST(TimeoutMonitor, MessageThatNeverArrivesIsLostOnceAfterItsTimeout)
{
    ecu::TimeoutMonitor<2> m(kCfg);
    m.start(1000);
    Recorder r;
    m.tick(1035, r);  // deadline is inclusive: not yet lost
    EXPECT_TRUE(r.lost.empty());
    m.tick(1036, r);
    ASSERT_EQ(r.lost.size(), 1U);
    EXPECT_EQ(r.lost[0], 0x100U);
    m.tick(1200, r);  // not reported again
    EXPECT_EQ(r.lost.size(), 1U);
    m.tick(1351, r);
    ASSERT_EQ(r.lost.size(), 2U);
    EXPECT_EQ(r.lost[1], 0x200U);
    EXPECT_EQ(m.find(0x100)->timeouts, 1U);
    EXPECT_FALSE(m.alive(0x100));
    EXPECT_TRUE(m.lost(0x100));
    EXPECT_EQ(m.age_ms(0x100, 2000), -1);  // never received
}

TEST(TimeoutMonitor, WaitingIsNeitherAliveNorLost)
{
    ecu::TimeoutMonitor<2> m(kCfg);
    m.start(0);
    EXPECT_FALSE(m.alive(0x100));
    EXPECT_FALSE(m.lost(0x100));
    const auto ignore = [](const ecu::TimeoutMonitor<2>::Entry&) {};
    m.tick(20, ignore);
    EXPECT_FALSE(m.lost(0x100)) << "still inside the first deadline";
    m.tick(36, ignore);
    EXPECT_TRUE(m.lost(0x100));
    m.note_received(0x100, 40);
    EXPECT_TRUE(m.alive(0x100));
    EXPECT_FALSE(m.lost(0x100));
}

TEST(TimeoutMonitor, ReceptionRenewsTheDeadlineAndLossIsDetectedAfterTimeout)
{
    ecu::TimeoutMonitor<2> m(kCfg);
    m.start(0);
    Recorder r;
    for (std::int64_t t = 10; t <= 100; t += 10) {
        EXPECT_FALSE(m.note_received(0x100, t));
        m.tick(t + 5, r);
    }
    EXPECT_TRUE(r.lost.empty());
    EXPECT_TRUE(m.alive(0x100));
    EXPECT_EQ(m.find(0x100)->received, 10U);
    EXPECT_EQ(m.age_ms(0x100, 105), 5);

    m.tick(135, r);  // last rx at 100, timeout 35 -> deadline 135, still alive
    EXPECT_TRUE(r.lost.empty());
    m.tick(136, r);
    ASSERT_EQ(r.lost.size(), 1U);
    EXPECT_FALSE(m.alive(0x100));
    EXPECT_EQ(m.find(0x100)->last_loss_ms, 136);

    // First frame after the loss is a recovery.
    EXPECT_TRUE(m.note_received(0x100, 500));
    EXPECT_TRUE(m.alive(0x100));
    EXPECT_FALSE(m.note_received(0x100, 510));
}

TEST(TimeoutMonitor, AllAliveAndUnknownIds)
{
    ecu::TimeoutMonitor<2> m(kCfg);
    m.start(0);
    EXPECT_FALSE(m.all_alive());
    m.note_received(0x100, 1);
    m.note_received(0x200, 1);
    EXPECT_TRUE(m.all_alive());
    EXPECT_FALSE(m.note_received(0x7E0, 1));  // not monitored
    EXPECT_EQ(m.find(0x7E0), nullptr);
    EXPECT_FALSE(m.alive(0x7E0));
    EXPECT_EQ(m.age_ms(0x7E0, 10), -1);
}

TEST(TimeoutMonitor, ConfigurationFromRuntimeValues)
{
    // A configuration the compiler cannot fold (the firmware's role tables are
    // constants; on a target without constant initialisation the constructor
    // runs at boot).
    volatile std::uint32_t timeout = 70;
    const std::array<ecu::MonitoredMessage, 2> cfg{{{0x080, timeout, "CONTROLLER_COMMAND"}, {0x201, 350, "CONTROLLER_HEARTBEAT"}}};
    ecu::TimeoutMonitor<2> m(cfg);
    EXPECT_EQ(m.find(0x080)->cfg.timeout_ms, 70U);
    EXPECT_STREQ(m.entries()[1].cfg.name, "CONTROLLER_HEARTBEAT");
    m.start(0);
    Recorder r;
    m.tick(70, r);
    EXPECT_TRUE(r.lost.empty());
    m.tick(71, r);
    ASSERT_EQ(r.lost.size(), 1U);
    EXPECT_EQ(r.lost[0], 0x080U);
}

TEST(TimeoutMonitor, WorksAtCompileTime)
{
    constexpr auto scenario = [] {
        ecu::TimeoutMonitor<2> m(kCfg);
        m.start(0);
        m.note_received(0x100, 5);
        int lost = 0;
        m.tick(100, [&](const ecu::TimeoutMonitor<2>::Entry&) { ++lost; });
        return lost == 1 && !m.alive(0x100) && !m.alive(0x200);
    };
    static_assert(scenario(), "constexpr timeout monitor");
    EXPECT_TRUE(scenario()) << "and the same code at run time";
}

}  // namespace

#include <gtest/gtest.h>

#include "ecu/error_counter_model.hpp"

namespace {

using ecu::ErrorCounterModel;
using Event = ErrorCounterModel::Event;
using proto::CanState;

TEST(ErrorCounterModel, TransmitErrorsWalkThroughPassiveToBusOff)
{
    ErrorCounterModel m;
    EXPECT_EQ(m.state(), CanState::kErrorActive);
    for (int i = 0; i < 12; ++i) {
        m.apply(Event::kTxError);  // TEC 96
    }
    EXPECT_EQ(m.tec(), 96);
    EXPECT_TRUE(m.warning());
    EXPECT_EQ(m.state(), CanState::kErrorActive);
    for (int i = 0; i < 4; ++i) {
        m.apply(Event::kTxError);  // TEC 128
    }
    EXPECT_EQ(m.tec(), 128);
    EXPECT_EQ(m.state(), CanState::kErrorPassive);
    for (int i = 0; i < 15; ++i) {
        m.apply(Event::kTxError);  // TEC 248
    }
    EXPECT_EQ(m.state(), CanState::kErrorPassive);
    EXPECT_FALSE(m.bus_off());
    m.apply(Event::kTxError);  // TEC 256 > 255
    EXPECT_TRUE(m.bus_off());
    EXPECT_EQ(m.state(), CanState::kBusOff);
    EXPECT_EQ(m.tec(), 255) << "reported counter saturates at 8 bits";
    m.apply(Event::kTxSuccess);  // frozen while bus-off
    EXPECT_EQ(m.state(), CanState::kBusOff);
    m.recover();
    EXPECT_EQ(m.state(), CanState::kErrorActive);
    EXPECT_EQ(m.tec(), 0);
    EXPECT_EQ(m.rec(), 0);
}

TEST(ErrorCounterModel, SuccessfulTransmissionsHeal)
{
    ErrorCounterModel m;
    for (int i = 0; i < 16; ++i) {
        m.apply(Event::kTxError);  // 128, passive
    }
    m.apply(Event::kTxSuccess);  // 127: active again
    EXPECT_EQ(m.state(), CanState::kErrorActive);
    EXPECT_TRUE(m.warning());
    for (int i = 0; i < 200; ++i) {
        m.apply(Event::kTxSuccess);
    }
    EXPECT_EQ(m.tec(), 0) << "TEC does not go below zero";
    EXPECT_FALSE(m.warning());
}

TEST(ErrorCounterModel, ReceiveErrorsSaturateAbovePassiveAndFallBackTo127)
{
    ErrorCounterModel m;
    for (int i = 0; i < 128; ++i) {
        m.apply(Event::kRxError);
    }
    EXPECT_EQ(m.rec(), 128);
    EXPECT_EQ(m.state(), CanState::kErrorPassive);
    m.apply(Event::kRxError);
    EXPECT_EQ(m.rec(), 128) << "REC above 127 is not increased further";
    m.apply(Event::kRxSuccess);
    EXPECT_EQ(m.rec(), 127) << "a successful reception brings REC back to 127";
    EXPECT_EQ(m.state(), CanState::kErrorActive);
    m.apply(Event::kRxSuccess);
    EXPECT_EQ(m.rec(), 126);
    EXPECT_FALSE(m.bus_off()) << "receive errors alone never cause bus-off";
}

TEST(ErrorCounterModel, WorksAtCompileTime)
{
    constexpr auto state = [] {
        ErrorCounterModel m;
        for (int i = 0; i < 32; ++i) {
            m.apply(Event::kTxError);
        }
        return m.state();
    }();
    static_assert(state == CanState::kBusOff, "32 transmit errors are bus-off");
}

}  // namespace

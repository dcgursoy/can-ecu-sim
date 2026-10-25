#include <array>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "protocol/codec_util.hpp"

namespace {

using Frame = std::array<std::uint8_t, 8>;
using proto::codec::get_signed;
using proto::codec::get_unsigned;
using proto::codec::phys_to_raw;
using proto::codec::put;
using proto::codec::raw_to_phys;

TEST(CodecUtil, PutThenGetRoundTripsAcrossByteBoundaries)
{
    Frame d{};
    put(d, 4, 12, 0xABC);   // straddles byte 0 and 1
    put(d, 17, 7, 0x55);    // inside byte 2, not byte aligned
    put(d, 24, 32, 0xDEADBEEF);
    put(d, 61, 3, 0x7);
    EXPECT_EQ(get_unsigned(d, 4, 12), 0xABCU);
    EXPECT_EQ(get_unsigned(d, 17, 7), 0x55U);
    EXPECT_EQ(get_unsigned(d, 24, 32), 0xDEADBEEFU);
    EXPECT_EQ(get_unsigned(d, 61, 3), 0x7U);
    // Untouched bits stay zero.
    EXPECT_EQ(get_unsigned(d, 0, 4), 0U);
    EXPECT_EQ(get_unsigned(d, 16, 1), 0U);
}

TEST(CodecUtil, PutMasksToFieldWidthAndClearsOldBits)
{
    Frame d{};
    put(d, 8, 4, 0xFF);  // only 4 bits are written
    EXPECT_EQ(d[1], 0x0F);
    put(d, 8, 4, 0x2);   // old bits cleared, not or-ed
    EXPECT_EQ(d[1], 0x02);
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[2], 0);
}

TEST(CodecUtil, SignedFieldsSignExtend)
{
    Frame d{};
    put(d, 0, 16, static_cast<std::uint32_t>(static_cast<std::int16_t>(-3)));
    EXPECT_EQ(get_signed(d, 0, 16), -3);
    put(d, 16, 4, 0x8);  // 4-bit -8
    EXPECT_EQ(get_signed(d, 16, 4), -8);
    put(d, 20, 4, 0x7);
    EXPECT_EQ(get_signed(d, 20, 4), 7);
    put(d, 32, 32, 0x80000000U);
    EXPECT_EQ(get_signed(d, 32, 32), std::numeric_limits<std::int32_t>::min());
    put(d, 32, 32, 0xFFFFFFFFU);
    EXPECT_EQ(get_signed(d, 32, 32), -1);
}

TEST(CodecUtil, PhysicalConversionRoundsAndSaturates)
{
    EXPECT_FLOAT_EQ(raw_to_phys(1234, 0.01F, 0.0F), 12.34F);
    EXPECT_FLOAT_EQ(raw_to_phys(0, 1.0F, -40.0F), -40.0F);
    EXPECT_EQ(phys_to_raw(12.34F, 0.01F, 0.0F, 0, 65535), 1234);
    EXPECT_EQ(phys_to_raw(12.344F, 0.01F, 0.0F, 0, 65535), 1234);   // rounds down
    EXPECT_EQ(phys_to_raw(12.346F, 0.01F, 0.0F, 0, 65535), 1235);   // rounds up
    EXPECT_EQ(phys_to_raw(-12.346F, 0.01F, 0.0F, -32768, 32767), -1235);
    EXPECT_EQ(phys_to_raw(1.0e9F, 0.01F, 0.0F, 0, 65535), 65535);   // saturates high
    EXPECT_EQ(phys_to_raw(-1.0F, 0.01F, 0.0F, 0, 65535), 0);        // saturates low
    EXPECT_EQ(phys_to_raw(-1.0e9F, 0.01F, 0.0F, -32768, 32767), -32768);
    EXPECT_EQ(phys_to_raw(25.0F, 1.0F, -40.0F, 0, 255), 65);
}

TEST(CodecUtil, PhysicalConversionRoundsExactlyOnce)
{
    // 0.49999997F + 0.5F would round up to 1.0F in float arithmetic; the nearest step is 0.
    EXPECT_EQ(phys_to_raw(0.49999997F, 1.0F, 0.0F, 0, 255), 0);
    EXPECT_EQ(phys_to_raw(0.5F, 1.0F, 0.0F, 0, 255), 1);          // halves away from zero
    EXPECT_EQ(phys_to_raw(2.5F, 1.0F, 0.0F, 0, 255), 3);
    EXPECT_EQ(phys_to_raw(-2.5F, 1.0F, 0.0F, -128, 127), -3);
    EXPECT_EQ(phys_to_raw(-2.4F, 1.0F, 0.0F, -128, 127), -2);
    EXPECT_EQ(phys_to_raw(254.9F, 1.0F, 0.0F, 0, 255), 255);
}

TEST(CodecUtil, PhysicalConversionHandlesNaNAndInfinity)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(phys_to_raw(nan, 0.01F, 0.0F, 0, 65535), 0);
    EXPECT_EQ(phys_to_raw(nan, 0.01F, 0.0F, -32768, 32767), -32768);
    EXPECT_EQ(phys_to_raw(inf, 0.01F, 0.0F, 0, 65535), 65535);
    EXPECT_EQ(phys_to_raw(-inf, 0.01F, 0.0F, 0, 65535), 0);
}

TEST(CodecUtil, WorksAtCompileTime)
{
    constexpr Frame d = [] {
        Frame f{};
        put(f, 3, 9, 0x1AB);
        return f;
    }();
    static_assert(get_unsigned(d, 3, 9) == 0x1ABU, "constexpr round trip");
    static_assert(phys_to_raw(5.0F, 0.5F, 0.0F, 0, 200) == 10, "constexpr conversion");
}

}  // namespace

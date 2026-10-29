#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "generated/test_vectors.hpp"
#include "protocol/crc8.hpp"
#include "protocol/e2e.hpp"

namespace {

using proto::crc8::compute;

std::uint8_t crc_of_string(const char* text)
{
    return compute(reinterpret_cast<const std::uint8_t*>(text), std::strlen(text));
}

TEST(Crc8, CheckValue)
{
    EXPECT_EQ(crc_of_string("123456789"), proto::crc8::kCheckValue);
}

// Check values published in the AUTOSAR CRC Library specification for CRC8H2F,
// an independent source for the polynomial, init and final XOR.
TEST(Crc8, AutosarCheckValues)
{
    struct Case {
        std::vector<std::uint8_t> data;
        std::uint8_t crc;
    };
    const Case cases[] = {
        {{0x00, 0x00, 0x00, 0x00}, 0x12},
        {{0xF2, 0x01, 0x83}, 0xC2},
        {{0x0F, 0xAA, 0x00, 0x55}, 0xC6},
        {{0x00, 0xFF, 0x55, 0x11}, 0x77},
        {{0x33, 0x22, 0x55, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, 0x11},
        {{0x92, 0x6B, 0x55}, 0x33},
        {{0xFF, 0xFF, 0xFF, 0xFF}, 0x6C},
    };
    for (const auto& c : cases) {
        EXPECT_EQ(compute(c.data.data(), c.data.size()), c.crc);
    }
}

TEST(Crc8, TableMatchesBitwiseDefinition)
{
    for (unsigned i = 0; i < 256; ++i) {
        EXPECT_EQ(proto::crc8::kTable[i], proto::crc8::update_bitwise(0, static_cast<std::uint8_t>(i)));
    }
    // Incremental use equals one-shot use.
    proto::crc8::Crc8 incremental;
    for (const char* p = "123456789"; *p != '\0'; ++p) {
        incremental.update(static_cast<std::uint8_t>(*p));
    }
    EXPECT_EQ(incremental.value(), proto::crc8::kCheckValue);
    incremental.reset();
    EXPECT_EQ(incremental.value(), static_cast<std::uint8_t>(proto::crc8::kInitialValue ^ proto::crc8::kFinalXor));
}

TEST(Crc8, IsComputedAtCompileTime)
{
    constexpr std::array<std::uint8_t, 9> digits{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    static_assert(compute(digits) == proto::crc8::kCheckValue, "constexpr CRC must match the check value");
    // The same overload at run time, on data the compiler cannot fold.
    const auto copy = digits;
    EXPECT_EQ(compute(copy), proto::crc8::kCheckValue);
    // The table is built once at compile time; building it at run time gives the
    // same bytes (called through a volatile pointer so the compiler cannot fold
    // the constexpr call into its constant).
    std::array<std::uint8_t, 256> (*volatile build)() = &proto::crc8::make_table;
    const auto built = build();
    EXPECT_EQ(built, proto::crc8::kTable);
}

// The Python reference (can/e2e_ref.py) computed the CRC of random frames
// including the identifier bytes; the C++ frame CRC must agree on all of them.
TEST(Crc8, FrameCrcMatchesPythonReference)
{
    for (const auto& v : vectors::kCrc) {
        EXPECT_EQ(proto::e2e::compute_crc(v.id, v.bytes), v.crc) << "id 0x" << std::hex << v.id;
    }
}

}  // namespace

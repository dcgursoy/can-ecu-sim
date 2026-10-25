// CRC-8H2F: polynomial 0x2F, initial value 0xFF, final XOR 0xFF, no reflection.
// This is the second CRC-8 of the AUTOSAR CRC library (Crc_CalculateCRC8H2F),
// used by E2E profile 2; its check value for "123456789" is 0xDF.
//
// Chosen over the SAE J1850 polynomial 0x1D (E2E profile 1) by computing the
// undetected error patterns for this project's 80-bit codeword (72 protected
// data bits + 8 CRC bits, can/crc_hd.py): 0x2F detects every 1-, 2- and 3-bit
// error (Hamming distance 4), 0x1D misses 362 of the 82,160 possible 3-bit
// errors (Hamming distance 3). See DESIGN.md section 10.2.
//
// Header-only, constexpr, no allocation. The lookup table is built at compile
// time from the bitwise definition, so the two can be checked against each
// other in the unit tests.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace proto::crc8 {

constexpr std::uint8_t kPolynomial = 0x2F;
constexpr std::uint8_t kInitialValue = 0xFF;
constexpr std::uint8_t kFinalXor = 0xFF;
constexpr std::uint8_t kCheckValue = 0xDF;  // crc8("123456789")

/// One byte through the bitwise definition (used to build the table and in tests).
constexpr std::uint8_t update_bitwise(std::uint8_t crc, std::uint8_t byte)
{
    crc = static_cast<std::uint8_t>(crc ^ byte);
    for (int bit = 0; bit < 8; ++bit) {
        const bool msb = (crc & 0x80U) != 0;
        crc = static_cast<std::uint8_t>(crc << 1);
        if (msb) {
            crc = static_cast<std::uint8_t>(crc ^ kPolynomial);
        }
    }
    return crc;
}

constexpr std::array<std::uint8_t, 256> make_table()
{
    std::array<std::uint8_t, 256> table{};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = update_bitwise(0, static_cast<std::uint8_t>(i));
    }
    return table;
}

inline constexpr std::array<std::uint8_t, 256> kTable = make_table();

/// Incremental CRC: feed bytes with update(), read the result with value().
class Crc8 {
public:
    constexpr void update(std::uint8_t byte)
    {
        crc_ = kTable[static_cast<std::uint8_t>(crc_ ^ byte)];
    }

    constexpr void update(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i) {
            update(data[i]);
        }
    }

    /// Final CRC (final XOR applied). The object can keep being updated.
    constexpr std::uint8_t value() const
    {
        return static_cast<std::uint8_t>(crc_ ^ kFinalXor);
    }

    constexpr void reset()
    {
        crc_ = kInitialValue;
    }

private:
    std::uint8_t crc_ = kInitialValue;
};

/// CRC of a complete buffer.
constexpr std::uint8_t compute(const std::uint8_t* data, std::size_t len)
{
    Crc8 crc;
    crc.update(data, len);
    return crc.value();
}

template <std::size_t N>
constexpr std::uint8_t compute(const std::array<std::uint8_t, N>& data)
{
    return compute(data.data(), N);
}

}  // namespace proto::crc8

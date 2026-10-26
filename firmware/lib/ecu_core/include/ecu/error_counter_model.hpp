// Model of a CAN controller's fault-confinement counters (ISO 11898-1, clause
// on error counting), used by the simulator's HAL test hook to turn injected
// error events into the controller states a real peripheral would report.
// Virtual CAN never produces electrical errors, so this is how error-passive
// and bus-off are exercised without hardware.
//
// Rules implemented (the common cases; the special +8 receive cases for a node
// that detected the error first are folded into kRxError):
//   transmit error      TEC += 8
//   transmit success    TEC -= 1 (if > 0)
//   receive error       REC += 1 (REC above 127 is not increased further)
//   receive success     REC -= 1, or REC := 127 if it was above 127
//   state               bus-off if TEC > 255, error-passive if TEC > 127 or
//                       REC > 127, else error-active (warning at >= 96)
//   bus-off recovery    TEC := 0, REC := 0 after 128 occurrences of 11
//                       recessive bits; the HAL models that delay
#pragma once

#include <cstdint>

#include "protocol/messages.hpp"

namespace ecu {

class ErrorCounterModel {
public:
    enum class Event : std::uint8_t { kTxError, kTxSuccess, kRxError, kRxSuccess };

    static constexpr std::uint16_t kWarningLimit = 96;
    static constexpr std::uint16_t kPassiveLimit = 127;
    static constexpr std::uint16_t kBusOffLimit = 255;

    constexpr void apply(Event event)
    {
        if (bus_off_) {
            return;  // counters are frozen until the recovery sequence completes
        }
        switch (event) {
        case Event::kTxError:
            tec_ = static_cast<std::uint16_t>(tec_ + 8U);
            if (tec_ > kBusOffLimit) {
                bus_off_ = true;
            }
            break;
        case Event::kTxSuccess:
            if (tec_ > 0U) {
                --tec_;
            }
            break;
        case Event::kRxError:
            if (rec_ <= kPassiveLimit) {
                ++rec_;
            }
            break;
        case Event::kRxSuccess:
            if (rec_ > kPassiveLimit) {
                rec_ = kPassiveLimit;
            } else if (rec_ > 0U) {
                --rec_;
            }
            break;
        }
    }

    /// Completion of the bus-off recovery sequence.
    constexpr void recover()
    {
        tec_ = 0;
        rec_ = 0;
        bus_off_ = false;
    }

    constexpr proto::CanState state() const
    {
        if (bus_off_) {
            return proto::CanState::kBusOff;
        }
        if (tec_ > kPassiveLimit || rec_ > kPassiveLimit) {
            return proto::CanState::kErrorPassive;
        }
        return proto::CanState::kErrorActive;
    }

    constexpr bool warning() const { return tec_ >= kWarningLimit || rec_ >= kWarningLimit; }
    constexpr bool bus_off() const { return bus_off_; }
    /// Counters as a driver reports them (8-bit; TEC saturates at 255 in bus-off).
    constexpr std::uint8_t tec() const { return static_cast<std::uint8_t>(tec_ > 255U ? 255U : tec_); }
    constexpr std::uint8_t rec() const { return static_cast<std::uint8_t>(rec_ > 255U ? 255U : rec_); }

private:
    std::uint16_t tec_ = 0;
    std::uint16_t rec_ = 0;
    bool bus_off_ = false;
};

}  // namespace ecu

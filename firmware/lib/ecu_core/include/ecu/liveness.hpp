// Thread liveness for the watchdog: every supervised thread checks in on each
// iteration of its loop; the supervisor feeds the hardware watchdog only when
// every thread has checked in within the window. A single stuck thread
// therefore resets the ECU even though the supervisor itself is still running,
// which is the property a plain "feed the watchdog from a timer" would lose.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ecu {

template <std::size_t N>
class Liveness {
public:
    constexpr explicit Liveness(std::uint32_t window_ms) : window_ms_(window_ms)
    {
        for (auto& t : last_checkin_ms_) {
            t = -1;
        }
    }

    constexpr void check_in(std::size_t thread, std::int64_t now_ms)
    {
        if (thread < N) {
            last_checkin_ms_[thread] = now_ms;
        }
    }

    /// Bit i set: thread i has not checked in within the window (or never).
    constexpr std::uint32_t stale_mask(std::int64_t now_ms) const
    {
        std::uint32_t mask = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (last_checkin_ms_[i] < 0 || now_ms - last_checkin_ms_[i] > window_ms_) {
                mask |= 1U << i;
            }
        }
        return mask;
    }

    constexpr bool all_alive(std::int64_t now_ms) const { return stale_mask(now_ms) == 0U; }
    constexpr std::uint32_t window_ms() const { return window_ms_; }

private:
    std::uint32_t window_ms_;
    std::array<std::int64_t, N> last_checkin_ms_{};
};

}  // namespace ecu

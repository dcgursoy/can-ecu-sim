// Message timeout monitor: tracks when each expected message was last received
// and reports loss and recovery transitions. Platform independent, no
// allocation; time is passed in (milliseconds since boot) so the logic can be
// unit tested without a clock.
//
// Semantics: every entry starts with a deadline of start + timeout, so a message
// that never arrives after boot is reported lost exactly once, after its
// timeout. A received frame renews the deadline; the first frame after a loss
// is reported as a recovery.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ecu {

struct MonitoredMessage {
    std::uint32_t id;
    std::uint32_t timeout_ms;
    const char* name;
};

template <std::size_t N>
class TimeoutMonitor {
public:
    struct Entry {
        MonitoredMessage cfg{};
        std::int64_t deadline_ms = 0;   ///< receive by this time or be declared lost
        std::int64_t last_rx_ms = -1;   ///< -1 until the first frame
        bool alive = false;             ///< true between a reception and its timeout
        bool lost_reported = false;     ///< the current loss has been reported
        std::uint32_t received = 0;
        std::uint32_t timeouts = 0;     ///< loss events (including "never arrived")
        std::int64_t last_loss_ms = -1;
    };

    constexpr explicit TimeoutMonitor(const std::array<MonitoredMessage, N>& cfg)
    {
        for (std::size_t i = 0; i < N; ++i) {
            entries_[i].cfg = cfg[i];
        }
    }

    /// Arm every deadline relative to `now_ms` (call once at startup).
    constexpr void start(std::int64_t now_ms)
    {
        for (auto& e : entries_) {
            e.deadline_ms = now_ms + e.cfg.timeout_ms;
            e.alive = false;
            e.lost_reported = false;
        }
    }

    /// Record a reception. Returns true if this frame ends a loss (recovery).
    constexpr bool note_received(std::uint32_t id, std::int64_t now_ms)
    {
        Entry* e = find_mut(id);
        if (e == nullptr) {
            return false;
        }
        const bool recovered = e->lost_reported;
        e->last_rx_ms = now_ms;
        e->deadline_ms = now_ms + e->cfg.timeout_ms;
        e->alive = true;
        e->lost_reported = false;
        ++e->received;
        return recovered;
    }

    /// Advance time; `on_lost(entry)` is called once per newly lost message.
    template <class F>
    constexpr void tick(std::int64_t now_ms, F&& on_lost)
    {
        for (auto& e : entries_) {
            if (!e.lost_reported && now_ms > e.deadline_ms) {
                e.alive = false;
                e.lost_reported = true;
                ++e.timeouts;
                e.last_loss_ms = now_ms;
                on_lost(static_cast<const Entry&>(e));
            }
        }
    }

    constexpr bool alive(std::uint32_t id) const
    {
        const Entry* e = find(id);
        return e != nullptr && e->alive;
    }

    /// True once the message's deadline has passed without a reception (the loss
    /// was reported) and no frame has arrived since. False while still waiting
    /// for the first frame after start().
    constexpr bool lost(std::uint32_t id) const
    {
        const Entry* e = find(id);
        return e != nullptr && e->lost_reported;
    }

    constexpr bool all_alive() const
    {
        for (const auto& e : entries_) {
            if (!e.alive) {
                return false;
            }
        }
        return true;
    }

    /// Milliseconds since the last reception, or -1 if nothing was received yet.
    constexpr std::int64_t age_ms(std::uint32_t id, std::int64_t now_ms) const
    {
        const Entry* e = find(id);
        return (e == nullptr || e->last_rx_ms < 0) ? -1 : now_ms - e->last_rx_ms;
    }

    constexpr const Entry* find(std::uint32_t id) const
    {
        for (const auto& e : entries_) {
            if (e.cfg.id == id) {
                return &e;
            }
        }
        return nullptr;
    }

    constexpr const std::array<Entry, N>& entries() const { return entries_; }

private:
    constexpr Entry* find_mut(std::uint32_t id)
    {
        for (auto& e : entries_) {
            if (e.cfg.id == id) {
                return &e;
            }
        }
        return nullptr;
    }

    std::array<Entry, N> entries_{};
};

}  // namespace ecu

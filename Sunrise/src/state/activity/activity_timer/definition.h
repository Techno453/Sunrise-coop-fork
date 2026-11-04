#pragma once
#include <cstdint>
namespace sunrise::state::activity::activity_timer {
/** A match or round timer in shared activity ticks, as the type-18 timer slot publishes it. */
struct ResolvedTimer final {
    /** Full duration. */
    std::uint64_t maximum{};
    /** Time left at `epoch`. */
    std::uint64_t remaining{};
    /** Time elapsed at `epoch`. */
    std::uint64_t currentAtEpoch{};
    /** Shared activity time the values above hold at. */
    std::uint64_t epoch{};
    /** Countdown speed; zero holds the timer. */
    float rate{};
    /** The timer slot's own trailing value. */
    std::int32_t tailValue{};
};
} // namespace sunrise::state::activity::activity_timer

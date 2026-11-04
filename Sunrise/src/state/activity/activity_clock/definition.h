#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

namespace sunrise::state::activity::activity_clock {

/** Package-name prefix of the PvP activities that run a shared activity clock and a match. */
inline constexpr std::string_view kPvpPackagePrefix = "pvp_";

/** Native activity time (35F080/35FC40) advances 673,200 ticks per second. */
inline constexpr std::uint64_t kTicksPerSecond = 673'200;
/** That is exactly 3,366 ticks in every five milliseconds, so the conversion stays integral. */
inline constexpr std::uint64_t kStepMilliseconds = 5;
inline constexpr std::uint64_t kTicksPerStep = 3'366;
inline constexpr std::uint64_t kMillisecondsPerSecond = 1'000;
static_assert(kTicksPerStep * (kMillisecondsPerSecond / kStepMilliseconds) == kTicksPerSecond);
/** Shared message-2 world interval and native frame-count statistic cadence. */
inline constexpr std::uint32_t kSimulationFramesPerSecond = 30;
inline constexpr std::uint64_t kMaximumTicks = (std::numeric_limits<std::int64_t>::max)();

/** Shared by all recipients of one bounded activity record; cleared on release/reallocation. */
struct Anchor final {
    std::uint64_t originMilliseconds{};
    std::uint64_t lastTicks{};
    bool initialized{};
};

/** Integer conversion into the native 35F080/35FC40 time unit, saturating before overflow. */
[[nodiscard]] constexpr std::uint64_t milliseconds_to_ticks(std::uint64_t milliseconds) noexcept {
    const std::uint64_t quotient = milliseconds / kStepMilliseconds;
    if (quotient > kMaximumTicks / kTicksPerStep) {
        return kMaximumTicks;
    }
    const std::uint64_t whole = quotient * kTicksPerStep;
    const std::uint64_t tail =
        (milliseconds % kStepMilliseconds) * kTicksPerStep / kStepMilliseconds;
    return tail > kMaximumTicks - whole ? kMaximumTicks : whole + tail;
}

/** First publication defines zero; an older sample cannot rewind the clock. */
[[nodiscard]] constexpr std::uint64_t advance(Anchor& anchor,
                                              std::uint64_t nowMilliseconds) noexcept {
    if (!anchor.initialized) {
        anchor = {nowMilliseconds, 0, true};
    }
    const std::uint64_t elapsed = nowMilliseconds >= anchor.originMilliseconds
                                      ? nowMilliseconds - anchor.originMilliseconds
                                      : 0;
    const std::uint64_t ticks = milliseconds_to_ticks(elapsed);
    if (ticks > anchor.lastTicks) {
        anchor.lastTicks = ticks;
    }
    return anchor.lastTicks;
}

} // namespace sunrise::state::activity::activity_clock

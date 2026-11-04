#pragma once

#include <cstdint>
#include <string_view>

namespace sunrise::state::activity::activity_clock {

/**
 * Produces shared PvP activity time for the message-5 world clock field (world `+0x50`). The first
 * publication defines the origin and the clock never pauses (Sunrise policy). A failed lookup
 * leaves output untouched.
 */
[[nodiscard]] bool resolve(std::uint64_t sessionId,
                           std::string_view packageName,
                           std::uint64_t nowMilliseconds,
                           std::uint64_t& ticks) noexcept;

} // namespace sunrise::state::activity::activity_clock

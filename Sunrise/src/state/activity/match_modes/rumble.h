#pragma once
#include "definition.h"

namespace sunrise::state::activity::match_modes::rumble {
/** FNV-1 of `pvp_rumble`: the Rumble private-match mode key and settings row. */
inline constexpr std::uint32_t kMode = 0xC3122B73U;
[[nodiscard]] const Definition& definition() noexcept;

} // namespace sunrise::state::activity::match_modes::rumble

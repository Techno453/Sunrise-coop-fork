#pragma once
#include "definition.h"

namespace sunrise::state::activity::match_modes::control {

/** FNV-1 of `pvp_control`: the installed activity mode key and its settings row in `0x80BEA325`. */
inline constexpr std::uint32_t kMode = 0x0072D303U;

/**
 * Control: two native partitions contest three capture zones. A captured zone scores the
 * installed `score_control_zone_captured` for the capturing team and each capturing member. A
 * kill of an enemy player scores `base_kill_score`, plus the row's advantage bonus while the
 * killer's team holds two zones or its power play bonus while it holds all three, until a team
 * total reaches the score limit or the match time runs out.
 */
[[nodiscard]] const Definition& definition() noexcept;

} // namespace sunrise::state::activity::match_modes::control

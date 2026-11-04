#pragma once

#include <string_view>

#include "definition.h"

namespace sunrise::state::activity::activity_timer {
/**
 * Resolves authored seconds from the exact committed session selection.
 * The match runtime supplies start, completion and postgame timing policy.
 */
[[nodiscard]] bool resolve_selected(std::uint64_t sessionId,
                                    std::string_view packageName,
                                    ResolvedTimer& output) noexcept;
} // namespace sunrise::state::activity::activity_timer

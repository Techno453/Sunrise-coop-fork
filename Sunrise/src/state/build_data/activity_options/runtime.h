#pragma once

#include <span>

#include "definition.h"

namespace sunrise::state::build_data::activity_options {
/** Publishes one complete immutable installed table. Invalid input never changes it. */
[[nodiscard]] bool publish(std::span<const Option> options) noexcept;
/** Resolves a named integer from the descriptor's original 13-bit option pairs, in wire order. */
[[nodiscard]] bool
resolve(std::span<const std::uint16_t> pairs, std::uint32_t key, std::int32_t& value) noexcept;
} // namespace sunrise::state::build_data::activity_options

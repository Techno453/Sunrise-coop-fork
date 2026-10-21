#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::state::build_data::starter_sets {

/** The installed table holds one row for each of the 3 playable classes. */
inline constexpr std::size_t kClassCount = 3;
/** Items one class row lists over its two lists. The installed rows list 15. */
inline constexpr std::size_t kItemCapacity = 24;

/** The items the packages give a new character of one class, in authored order. */
struct Definition {
    /** Class byte the row is keyed by: 0 Titan, 1 Hunter, 2 Warlock. */
    std::uint8_t characterClass{};
    std::uint8_t itemCount{};
    std::array<std::uint16_t, kItemCapacity> itemIndices{};
};

} // namespace sunrise::state::build_data::starter_sets

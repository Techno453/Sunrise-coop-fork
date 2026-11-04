#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../middleware/content/packages/tables/activity_options.h"

namespace sunrise::state::build_data::activity_options {
inline constexpr std::size_t kOptionCapacity =
    middleware::content::packages::tables::activity_options::kOptionCapacity;
inline constexpr std::size_t kVariantCapacity =
    middleware::content::packages::tables::activity_options::kVariantCapacity;
/**
 * A descriptor option pair is 13 bits: the option ordinal above a 4-bit variant, each biased by
 * one. The descriptor's 6-bit count carries at most 63 pairs.
 */
inline constexpr std::uint16_t kMaximumPair = 0x1FFFU;
inline constexpr unsigned kPairVariantWidth = 4;
inline constexpr unsigned kPairVariantMask = 0xFU;
inline constexpr int kPairBias = 1;
inline constexpr std::size_t kPairCapacity = 63;

/** An installed option ordinal's authored variants, with missing values explicit. */
struct Option final {
    std::uint32_t settingKey{};
    std::uint8_t variantCount{};
    std::uint16_t presentMask{};
    std::array<std::int32_t, kVariantCapacity> values{};
};
} // namespace sunrise::state::build_data::activity_options

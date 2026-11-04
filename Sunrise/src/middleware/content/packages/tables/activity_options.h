#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../schema/generated/tag_classes_80807d.h"
#include "definition_index_table.h"

namespace sunrise::middleware::content::packages::tables::activity_options {

/** Private-match option table class, named by the investment root at `kRootSlot`. */
inline constexpr std::uint32_t kTableClass = schema::generated::C80807d6b::kClass;
inline constexpr std::size_t kRootSlot =
    root_slot(offsetof(schema::generated::InvestmentRoot, unk418));
/** Signed 9/4-bit fields, each with bias one, address these many nonnegative entries. */
inline constexpr std::size_t kOptionCapacity = 511;
inline constexpr std::size_t kVariantCapacity = 15;

struct Option final {
    std::uint32_t settingKey{};
    std::uint8_t variantCount{};
    std::uint16_t presentMask{};
    std::array<std::int32_t, kVariantCapacity> values{};
};
struct Catalog final {
    std::array<Option, kOptionCapacity> options{};
    std::uint16_t count{};
};

/** Returned bytes must remain valid until the next callback; never alias the table blob. */
using ReadTag = bool (*)(void*,
                         std::uint32_t,
                         std::span<const std::byte>&,
                         std::uint32_t&) noexcept;

/**
 * Reads the installed option table and its referenced integer time/score setting lists.
 * Unknown option kinds retain their ordinal and variant bound, without inferred values.
 * On any malformed target list or failed child read, output remains unchanged.
 */
[[nodiscard]] bool
parse(std::span<const std::byte> table, void* context, ReadTag readTag, Catalog& output) noexcept;

} // namespace sunrise::middleware::content::packages::tables::activity_options

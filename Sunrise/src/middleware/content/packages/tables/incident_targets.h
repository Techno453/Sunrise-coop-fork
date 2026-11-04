#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../schema/generated/tag_classes_80807c.h"
#include "definition_index_table.h"

namespace sunrise::middleware::content::packages::tables::incident_targets {

/** The installed incident target bank: the sobject definition table under its alternate tag. */
inline constexpr std::uint32_t kCatalogTag = kSobjectTableAlternateTag;
inline constexpr std::uint32_t kCatalogClass = schema::generated::C80807c9b::kClass;
/**
 * Rows of the installed target bank (`kCatalogTag`) of the supported client content, as captured
 * in the incident-targets fixture. Rules and selectors name targets by row index, so a bank with
 * another row count is other content and publishes no match rules.
 */
inline constexpr std::size_t kInstalledTargetCount = 7763;
/** Bank capacities: every installed target, presentation group and selector node. */
inline constexpr std::size_t kTargetCapacity = kInstalledTargetCount;
inline constexpr std::size_t kGroupCapacity = 16;
inline constexpr std::size_t kNodeCapacity = 8192;
/** Nodes one presentation group holds. */
inline constexpr std::size_t kGroupNodeCapacity = 2048;
/** Native completed event-list capacity, independent of the 25 explicit extra targets. */
inline constexpr std::size_t kSelectionCapacity = 64;

/** Payload type codes of `Target::type` the match reads, each with its native schema. */
inline constexpr std::int8_t kKillType = 1;        // C8080645a
inline constexpr std::int8_t kPlayerEventType = 3; // C808087a5, the `player` role
inline constexpr std::int8_t kCaptureType = 6;     // C80806470, a zone report
inline constexpr std::int8_t kSpawnType = 13;      // C80806437, `player_spawned`
inline constexpr std::int8_t kCinematicType = 17;  // C808087bf, a playback report
inline constexpr std::int8_t kFollowUpType = 20;   // C8080880c, a threshold follow-up
inline constexpr std::int8_t kAmountType = 25;     // C80806436, a score notice

struct Target final {
    std::uint32_t key{};
    std::int16_t group{-1};
    std::int16_t node{-1};
    std::int8_t type{-1};
};
struct Node final {
    std::uint16_t target{};
    std::uint16_t firstChild{};
    std::uint16_t childCount{};
};
struct Group final {
    std::uint16_t firstNode{};
    std::uint16_t nodeCount{};
};
struct Catalog final {
    std::array<Target, kTargetCapacity> targets{};
    std::array<Group, kGroupCapacity> groups{};
    std::array<Node, kNodeCapacity> nodes{};
    std::uint16_t targetCount{};
    std::uint16_t nodeCount{};
    std::uint8_t groupCount{};
};
/** One completed event list, in native traversal order. */
struct Selection final {
    std::array<std::uint16_t, kSelectionCapacity> targets{};
    std::uint8_t count{};
};

[[nodiscard]] bool parse(std::span<const std::byte> bytes, Catalog& output) noexcept;

/** Original 4A7350 traversal with checked references, bounded output and exact framing. */
[[nodiscard]] bool
expand(const Catalog& catalog, std::span<const std::byte> selector, Selection& output) noexcept;

} // namespace sunrise::middleware::content::packages::tables::incident_targets

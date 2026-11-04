#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../content/packages/schema/generated/tag_classes_808099.h"
#include "../../encoding/bit_writer.h"
#include "auth_fields.h"

namespace sunrise::middleware::bap::activity_message::scoreboard_record {

// --- Schema classes of the scoreboard record and of the auth-body carrier it rides in. Every
// width and count below is one of those two classes' own field declarations. ---

/** The scoreboard record's own schema class id. */
inline constexpr std::uint32_t kRecordClassId =
    content::packages::schema::generated::C808099f9::kClass;
/** Schema class id of the auth-body carrier this record rides in. */
inline constexpr std::uint32_t kComponentClassId =
    content::packages::schema::generated::C808099f1::kClass;
/** The wire slot type the global activity group publishes this component at. */
inline constexpr std::uint8_t kSlotType = 16;

/** Presence bits the record's root writes, one per declared field. */
inline constexpr std::size_t kRootFieldCount = 7;
/** Root presence-bit index of the element array. */
inline constexpr std::uint16_t kElementsFieldIndex = 0;
/** Root presence-bit index of the team list. */
inline constexpr std::uint16_t kTeamListFieldIndex = 1;
/** Root presence-bit index of the global statistics, which write their own presence bit. */
inline constexpr std::uint16_t kGlobalStatisticsFieldIndex = 2;

/** The element array declares its length in five bits and reserves sixteen seats. */
inline constexpr std::uint8_t kElementCountWidth = 5;
/** See kElementCountWidth. */
inline constexpr std::size_t kElementCount = 16;

/** One element is a 64-bit key behind its presence bit, with four declared fields between. */
inline constexpr std::uint8_t kElementKeyWidth = 64;
/** See kElementKeyWidth. */
inline constexpr std::size_t kElementMiddleFieldCount = 4;
/** Bits an unkeyed seat still writes: its own presence bit, the middle fields, and `live`. */
inline constexpr std::size_t kEmptyElementBits = 1 + kElementMiddleFieldCount + 1;
/** Bits a keyed seat writes: the empty seat's bits plus the key itself. */
inline constexpr std::size_t kKeyedElementBits = kEmptyElementBits + kElementKeyWidth;

// Native counted float arrays: player C80809a09 (64 values behind a 7-bit count) and team
// C8080968e (16 behind a 5-bit count). The native encoder emits each supplied value with
// presence=1 and its raw IEEE-754 bits. These are carriers only; the owning activity producer
// must supply the values and indices.
inline constexpr std::size_t kPlayerStatisticCapacity = 64;
inline constexpr std::uint8_t kPlayerStatisticCountWidth = 7;
inline constexpr std::size_t kTeamStatisticCapacity = 16;
inline constexpr std::uint8_t kTeamStatisticCountWidth = 5;
/** Native root field2: C808099ff counted array of at most24 global floats. */
inline constexpr std::size_t kGlobalStatisticCapacity = 24;
/** One statistic value: its presence bit, then its 32 raw bits. */
inline constexpr std::uint8_t kStatisticValueWidth = 32;
inline constexpr std::size_t kStatisticValueBits = 1 + kStatisticValueWidth;

/** Counted float array carrier for one element or team; see the note above. */
template <std::size_t Capacity> struct Statistics final {
    static_assert(Capacity == kPlayerStatisticCapacity || Capacity == kTeamStatisticCapacity
                  || Capacity == kGlobalStatisticCapacity);
    std::array<float, Capacity> values{};
    std::uint8_t count{};
    bool present{};
};

/** @return The count width of a statistic array of `Capacity` values. */
template <std::size_t Capacity>
[[nodiscard]] constexpr std::uint8_t statistic_count_width() noexcept {
    return Capacity == kPlayerStatisticCapacity ? kPlayerStatisticCountWidth
                                                : kTeamStatisticCountWidth;
}

template <std::size_t Capacity>
[[nodiscard]] constexpr std::size_t statistics_bits(const Statistics<Capacity>& stats) noexcept {
    return stats.present ? statistic_count_width<Capacity>()
                               + (std::size_t{stats.count} * kStatisticValueBits)
                         : 0U;
}

/** Optional element +0x12C: a 6-bit title ordinal biased by one, at most 62. */
inline constexpr std::uint8_t kHudTitleWidth = 6;
inline constexpr std::uint64_t kHudTitleBias = 1;
inline constexpr std::int8_t kLastHudTitle = 62;

/** Bits the element array's own length field costs, before any seat. */
inline constexpr std::size_t kElementsSubtreeBits = kElementCountWidth;

// C80809a00 has a four-bit count and at most twelve counted C80809a02 entries.
// Entry fields are optional signed faction (bias3/6bits), optional signed ordinal (bias1/6bits),
// non-optional live bit, then optional statistics. FDA3C0 admits only live entries into the HUD.
/** Root field 4 holds the 32-bit layout hash. */
inline constexpr std::uint16_t kLayoutFieldIndex = 4;
inline constexpr std::uint8_t kLayoutWidth = 32;
/** Scoreboard config 0x80BA0015's "match" layout, which 0xFD96F0 selects. */
inline constexpr std::uint32_t kMatchLayout = 0x5DEA3FF8U;

/** The team list declares its length in four bits and reserves twelve teams. */
inline constexpr std::uint8_t kTeamCountWidth = 4;
/** See kTeamCountWidth. */
inline constexpr std::size_t kTeamCount = 12;
/** An entry with every optional field absent: three clear presence bits and its live bit. */
inline constexpr std::size_t kNeutralTeamEntryBits = 4;
/** @return Bits `teamCount` bare entries cost, including the list's own length field. */
[[nodiscard]] constexpr std::size_t team_list_subtree_bits(std::uint8_t teamCount) noexcept {
    return kTeamCountWidth + std::size_t{teamCount} * kNeutralTeamEntryBits;
}

/** One scoreboard seat: a live entry carries a key, an absent one carries neither. */
struct Element final {

    std::uint64_t key{};
    bool live{};
    bool hasKey{};
    Statistics<kPlayerStatisticCapacity> statistics{};
    /** Optional element +0x12C: ordinal in the authored HUD title table; -1 is absent. */
    std::int8_t hudTitle{-1};
};

/** One player's live score, keyed by its player key. */
struct PlayerScore final {
    std::uint64_t playerKey{};
    float value{};
};

/** One partition's retained team score, keyed by its native partition. */
struct TeamScore final {
    std::int8_t faction{-1};
    float value{};
};

/** One player's statistic values, keyed by its player key. */
struct PlayerStatistics final {
    std::uint64_t playerKey{};
    std::array<float, kPlayerStatisticCapacity> values{};
};

/**
 * Native team entry, named by the team's faction (`auth_fields::team_faction`). The optional
 * ordinal remains absent until its producer supplies it.
 */
struct Team final {
    std::int8_t faction{auth_fields::kMinimumFaction};
    bool hasFaction{};
    bool live{};
    Statistics<kTeamStatisticCapacity> statistics{};
    /** FDA960's fallback when the viewing player's title is absent. */
    std::int8_t hudTitle{-1};
};

/** One scoreboard component: its element seats and, optionally, its team list. */
struct Record final {
    std::array<Element, kElementCount> elements{};
    std::uint8_t elementCount{};
    /** The authoritative factions represented by this record's live seats. */
    std::array<Team, kTeamCount> teams{};
    bool hasTeamList{};
    std::uint8_t teamCount{};
    /** Root field 4, native body+0x16F4: selects a layout from scoreboard config+0x30. */
    bool hasLayout{};
    std::uint32_t layoutHash{};
    Statistics<kGlobalStatisticCapacity> globalStatistics{};
};

/** @return Wire bits `record` costs past the root, for only the seats its own count declares. */
[[nodiscard]] constexpr std::size_t record_body_bits(const Record& record) noexcept {
    std::size_t bits = kRootFieldCount + kElementCountWidth;
    // Only the seats the count field declares reach the wire; the client stops there.
    const std::size_t seats =
        record.elementCount < kElementCount ? record.elementCount : kElementCount;
    for (std::size_t index = 0; index < seats; ++index) {
        bits += record.elements[index].hasKey ? kKeyedElementBits : kEmptyElementBits;
        bits += statistics_bits(record.elements[index].statistics);
        bits += record.elements[index].hudTitle >= 0 ? kHudTitleWidth : 0U;
    }
    if (record.hasTeamList) {
        bits += team_list_subtree_bits(record.teamCount);
        for (std::size_t index = 0; index < record.teamCount && index < kTeamCount; ++index) {
            bits += record.teams[index].hasFaction ? auth_fields::kFactionWidth : 0U;
            bits += statistics_bits(record.teams[index].statistics);
            bits += record.teams[index].hudTitle >= 0 ? kHudTitleWidth : 0U;
        }
    }
    return bits + statistics_bits(record.globalStatistics) + (record.hasLayout ? kLayoutWidth : 0U);
}

/** Shortest body a reader must still accept: the root plus an empty element array. */
inline constexpr std::size_t kMinimumRecordBits = kRootFieldCount + kElementsSubtreeBits;
static_assert(kMinimumRecordBits == 12);

/** @return False when `record` fails `valid`. */
[[nodiscard]] bool write_record_body(encoding::bits::Writer& writer, const Record& record) noexcept;

/**
 * @return False when `elementCount` or `teamCount` exceed their capacity, a seat's `live` and
 * `hasKey` disagree, a keyed seat's key is zero, or a seat past `elementCount` is not empty.
 */
[[nodiscard]] bool valid(const Record& record) noexcept;

} // namespace sunrise::middleware::bap::activity_message::scoreboard_record

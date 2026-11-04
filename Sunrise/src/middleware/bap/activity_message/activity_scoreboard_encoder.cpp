#include <bit>
#include <cmath>

#include "../../encoding/bit_writer.h"
#include "auth_schema_catalog.h"
#include "scoreboard_record.h"

namespace sunrise::middleware::bap::activity_message::scoreboard_record {
namespace {

namespace bits = encoding::bits;
constexpr std::uint8_t kPresenceWidth = 1;
namespace fields = auth_fields;
static_assert([] {
    for (const auto& row : auth_schema_catalog::kTypes) {
        if (row.slotType == kSlotType) {
            return row.schema == kRecordClassId && row.minimumBits == kRootFieldCount;
        }
    }
    return false;
}());

template <std::size_t Capacity>
[[nodiscard]] bool write_statistics(bits::Writer& writer,
                                    const Statistics<Capacity>& stats) noexcept {
    if (!writer.write(stats.present ? 1U : 0U, kPresenceWidth)) {
        return false;
    }
    if (!stats.present) {
        return true;
    }
    if (!writer.write(stats.count, statistic_count_width<Capacity>())) {
        return false;
    }
    for (std::size_t i = 0; i < stats.count; ++i) {
        if (!writer.write(1, kPresenceWidth)
            || !writer.write(std::bit_cast<std::uint32_t>(stats.values[i]), kStatisticValueWidth)) {
            return false;
        }
    }
    return true;
}

template <std::size_t Capacity>
[[nodiscard]] bool valid_statistics(const Statistics<Capacity>& stats) noexcept {
    if (stats.count > Capacity || (!stats.present && stats.count != 0)) {
        return false;
    }
    for (std::size_t i = 0; i < Capacity; ++i) {
        if (!std::isfinite(stats.values[i]) || (i >= stats.count && stats.values[i] != 0.0F)) {
            return false;
        }
    }
    return true;
}

/** Writes one 0x130-byte seat, retaining absent defaults for all unauthored fields. */
[[nodiscard]] bool write_element(bits::Writer& writer, const Element& element) noexcept {
    bool encoded = writer.write(element.hasKey ? 1U : 0U, kPresenceWidth);
    if (encoded && element.hasKey) {

        encoded = writer.write(element.key, kElementKeyWidth);
    }
    // The two middle fields before the statistics stay absent.
    encoded = encoded && writer.write(0, kPresenceWidth) && writer.write(0, kPresenceWidth)
              && write_statistics(writer, element.statistics)
              && writer.write(element.hudTitle >= 0 ? 1U : 0U, kPresenceWidth);
    if (encoded && element.hudTitle >= 0) {
        encoded = writer.write(static_cast<std::uint64_t>(element.hudTitle) + kHudTitleBias,
                               kHudTitleWidth);
    }
    return encoded && writer.write(element.live ? 1U : 0U, 1);
}

} // namespace

bool write_record_body(bits::Writer& writer, const Record& record) noexcept {
    if (!valid(record)) {
        return false;
    }
    bool encoded =
        writer.write(1, kPresenceWidth) && writer.write(record.elementCount, kElementCountWidth);
    for (std::size_t index = 0; encoded && index < record.elementCount; ++index) {
        encoded = write_element(writer, record.elements[index]);
    }
    encoded = encoded && writer.write(record.hasTeamList ? 1U : 0U, kPresenceWidth);
    if (encoded && record.hasTeamList) {
        encoded = writer.write(record.teamCount, kTeamCountWidth);
        for (std::size_t team = 0; encoded && team < record.teamCount; ++team) {
            const Team& entry = record.teams[team];
            encoded = writer.write(entry.hasFaction ? 1U : 0U, kPresenceWidth);
            if (encoded && entry.hasFaction) {
                encoded =
                    writer.write(static_cast<std::uint64_t>(entry.faction + fields::kFactionBias),
                                 fields::kFactionWidth);
            }
            encoded = encoded && writer.write(entry.hudTitle >= 0 ? 1U : 0U, kPresenceWidth);
            if (encoded && entry.hudTitle >= 0) {
                encoded = writer.write(static_cast<std::uint64_t>(entry.hudTitle) + kHudTitleBias,
                                       kHudTitleWidth);
            }
            encoded = encoded && writer.write(entry.live ? 1U : 0U, fields::kBoolWidth)
                      && write_statistics(writer, entry.statistics);
        }
    }
    // Global statistics and the authored display layout are optional; other fields remain absent.
    for (std::size_t field = kTeamListFieldIndex + 1; encoded && field < kRootFieldCount; ++field) {
        if (field == kGlobalStatisticsFieldIndex) {
            encoded = write_statistics(writer, record.globalStatistics);
            continue;
        }
        const bool present = field == kLayoutFieldIndex && record.hasLayout;
        encoded = writer.write(present ? 1U : 0U, kPresenceWidth);
        if (encoded && present) {
            encoded = writer.write(record.layoutHash, kLayoutWidth);
        }
    }
    return encoded;
}

bool valid(const Record& record) noexcept {
    if (!valid_statistics(record.globalStatistics)) {
        return false;
    }
    if (record.elementCount > kElementCount) {
        return false;
    }
    if (record.hasTeamList && record.teamCount > kTeamCount) {
        return false;
    }
    for (std::size_t index = 0; index < kTeamCount; ++index) {
        const Team& team = record.teams[index];
        if (team.hudTitle < -1 || team.hudTitle > kLastHudTitle
            || (team.hudTitle >= 0
                && (!team.live || !record.hasTeamList || index >= record.teamCount))) {
            return false;
        }
        if (!valid_statistics(team.statistics) || (team.statistics.present && !team.live)) {
            return false;
        }
        if (team.hasFaction
            && (team.faction < fields::kMinimumFaction || team.faction > fields::kMaximumFaction)) {
            return false;
        }
        if (team.live && (!team.hasFaction || team.faction < 0)) {
            return false;
        }
        if ((!record.hasTeamList || index >= record.teamCount) && (team.live || team.hasFaction)) {
            return false;
        }
        if (team.live) {
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (record.teams[previous].live && record.teams[previous].faction == team.faction) {
                    return false;
                }
            }
        }
    }
    for (std::size_t index = 0; index < kElementCount; ++index) {
        const Element& element = record.elements[index];
        if (element.hudTitle < -1 || element.hudTitle > kLastHudTitle
            || (element.hudTitle >= 0 && !element.live)) {
            return false;
        }
        if (!valid_statistics(element.statistics)
            || (element.statistics.present && !element.live)) {
            return false;
        }
        // A live seat with no key resolves to nothing and a keyed seat that is not live is
        // skipped by the block's own walker, so neither is worth putting on the wire.
        if (element.live != element.hasKey) {
            return false;
        }
        if (element.hasKey && element.key == 0) {
            return false;
        }
        // Only the first `elementCount` seats are encoded. A seat past the count would be dropped
        // in silence, and the record would claim a roster it never published.
        if (index >= record.elementCount && (element.live || element.hasKey)) {
            return false;
        }
    }
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::scoreboard_record

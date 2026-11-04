#include <array>

#include "../schema/generated/tag_classes_80804e.h"
#include "../schema/generated/tag_classes_808077.h"
#include "../schema/generated/tag_classes_80809a.h"
#include "activity_rules.h"
#include "activity_settings.h"
#include "activity_table.h"
#include "definition_index_table.h"
#include "field_reader.h"

namespace sunrise::middleware::content::packages::tables::activity_rules {
namespace {
namespace keys = activity_settings;
namespace scoreboards = statistic_program;
namespace generated = schema::generated;
using Bytes = std::span<const std::byte>;

/** Rows of the activity type table, which the client reads as mode families. */
constexpr std::size_t kActivityTypeRows = offsetof(generated::C80807773, table);
constexpr std::uint32_t kActivityTypeRowClass = generated::C80807778::kClass;
constexpr std::size_t kActivityTypeRowStride = sizeof(generated::C80807778);
constexpr std::size_t kActivityTypeCapacity = 256;
/** An activity type of 0xFF designates no mode. */
constexpr std::uint8_t kNoActivityType = 0xFF;
/** The activity definition table's index rows. */
constexpr std::size_t kActivityRows = offsetof(generated::ActivityDefinitionTable, table);

/** Mode settings table rows, each with its mode key and its typed setting list. */
constexpr std::size_t kModeRows = offsetof(generated::C80804e66, hAa10d60f);
constexpr std::uint32_t kModeRowClass = generated::C80804e68::kClass;
constexpr std::size_t kModeRowStride = sizeof(generated::C80804e68);
constexpr std::size_t kModeRowCapacity = 256;
constexpr std::size_t kModeKeyOffset = offsetof(generated::C80804e68, unk0);
constexpr std::size_t kModeSettingList = offsetof(generated::C80804e68, activityOptions.options);
constexpr std::size_t kModeSettingCapacity = 4096;

/**
 * Scoreboard title list: a localization tag, a title key and an arguments tag per row.
 * The row ordinal is the one-biased 6-bit wire title, so at most 63 rows can be named.
 */
constexpr std::size_t kTitleList = offsetof(generated::C80809a12, h855d36ce);
constexpr std::uint32_t kTitleRowClass = generated::C80809a1e::kClass;
constexpr std::size_t kTitleRowStride = sizeof(generated::C80809a1e);
constexpr std::size_t kTitleCapacity = 63;
constexpr std::size_t kTitleTagOffset = offsetof(generated::C80809a1e, unk0);
constexpr std::size_t kTitleKeyOffset = offsetof(generated::C80809a1e, unk4);
constexpr std::size_t kTitleArgumentsOffset = offsetof(generated::C80809a1e, unk8);

/** Found-mask bits of the nine decoded scoring and lifecycle settings. */
constexpr unsigned kFinalBlowSetting = 1U << 0U;
constexpr unsigned kKillSetting = 1U << 1U;
constexpr unsigned kScoreLimitSetting = 1U << 2U;
constexpr unsigned kFfaSetting = 1U << 3U;
constexpr unsigned kRoundLimitSetting = 1U << 4U;
constexpr unsigned kPostgameSetting = 1U << 5U;
constexpr unsigned kCompleteSetting = 1U << 6U;
constexpr unsigned kLandingSetting = 1U << 7U;
constexpr unsigned kTimeLimitSetting = 1U << 8U;
/**
 * Found mask once all nine settings are present; only the final blow score and the time limit
 * may be absent.
 */
constexpr unsigned kEverySetting = kFinalBlowSetting | kKillSetting | kScoreLimitSetting
                                   | kFfaSetting | kRoundLimitSetting | kPostgameSetting
                                   | kCompleteSetting | kLandingSetting | kTimeLimitSetting;
/** Settings a row may leave out. */
constexpr unsigned kOptionalSettings = kFinalBlowSetting | kTimeLimitSetting;

/**
 * Reads one typed table through the shared `read_array` descriptor reader. Even an empty table must
 * carry a header naming `cls`.
 */
bool array(Bytes bytes,
           std::size_t descriptor,
           std::uint32_t cls,
           std::size_t stride,
           std::size_t maximum,
           std::size_t& start,
           std::size_t& count) noexcept {
    Array rows{};
    if (!read_array(bytes, descriptor, cls, stride, rows) || rows.elementClass != cls
        || rows.count > maximum) {
        return false;
    }
    start = rows.dataOffset;
    count = static_cast<std::size_t>(rows.count);
    return true;
}
} // namespace

bool parse_activity_modes(Bytes activities, Bytes modes, ActivityModes& output) noexcept {
    ActivityModes result{};
    std::size_t act{}, actCount{}, mode{}, modeCount{};
    if (!array(activities,
               kActivityRows,
               activities::kRowClass,
               activities::kIndexRowStride,
               result.values.size(),
               act,
               actCount)
        || !array(modes,
                  kActivityTypeRows,
                  kActivityTypeRowClass,
                  kActivityTypeRowStride,
                  kActivityTypeCapacity,
                  mode,
                  modeCount)) {
        return false;
    }
    for (std::size_t i = 0; i < actCount; ++i) {
        std::size_t definition{};
        std::uint8_t type{};
        if (!relative(activities,
                      act + (i * activities::kIndexRowStride) + activities::kIndexRecordField,
                      definition)
            || !read(activities, definition + activities::kRecordTypeOffset, type)) {
            return false;
        }
        if (type == kNoActivityType) {
            continue;
        }
        // The activity type row is the mode family the client reads for the mode icon. The
        // definition's gameplay settings key names the settings row the mode definitions are keyed
        // by. Every private-match selection shares one type row, so only that key tells Rumble from
        // Control.
        if (type >= modeCount
            || !read(activities,
                     definition + activities::kRecordGameplaySettingsOffset,
                     result.values[i])) {
            return false;
        }
    }
    result.count = actCount;
    output = result;
    return true;
}

bool read_statistic_counts(Bytes scoreboard,
                           std::uint32_t& players,
                           std::uint32_t& teams) noexcept {
    std::uint32_t playerScore{}, teamScore{};
    return read(scoreboard, scoreboards::kPlayerStatisticCountOffset, players) && players != 0
           && players <= scoreboards::kPlayerStatisticCapacity
           && read(scoreboard,
                   scoreboards::kPlayerStatisticsOffset + scoreboards::kStatisticKeyOffset,
                   playerScore)
           && playerScore == scoreboards::kScoreStatistic
           && read(scoreboard, scoreboards::kTeamStatisticCountOffset, teams) && teams != 0
           && teams <= scoreboards::kTeamStatisticCapacity
           && read(scoreboard,
                   scoreboards::kTeamStatisticsOffset + scoreboards::kStatisticKeyOffset,
                   teamScore)
           && teamScore == scoreboards::kScoreStatistic
           && scoreboard.size() >= scoreboards::kPlayerStatisticsOffset
                                       + (players * scoreboards::kStatisticStride)
           && scoreboard.size()
                  >= scoreboards::kTeamStatisticsOffset + (teams * scoreboards::kStatisticStride);
}

bool parse_mode(Bytes settings,
                Bytes scoreboard,
                std::uint32_t modeKey,
                const PlacementKeys& keys,
                ModeData& output) noexcept {
    ModeData decoded{};
    auto& result = decoded.settings;
    std::size_t rows{}, rowCount{};
    if (modeKey == 0 || modeKey == keys::kEmptyNameKey
        || !array(
            settings, kModeRows, kModeRowClass, kModeRowStride, kModeRowCapacity, rows, rowCount)) {
        return false;
    }
    // The native per-player and faction score arrays use the authored column ordinal.
    // Preserve the complete authored vector; the native formatter skips missing ordinals.
    std::uint32_t playerCount{}, teamCount{};
    if (!read_statistic_counts(scoreboard, playerCount, teamCount)) {
        return false;
    }
    result.playerStatisticCount = playerCount;
    result.teamStatisticCount = teamCount;
    if (!statistic_program::parse(scoreboard, decoded.statistics)) {
        return false;
    }
    // Title keys come from the mode definition; a title's ordinal is its row in the installed
    // title list, never inferred from the order of roster seats.
    const auto& placeKeys = keys.places;
    const auto& tiedKeys = keys.ties;
    result.placementTitles.fill(-1);
    result.tiedPlacementTitles.fill(-1);
    std::size_t titles{}, titleCount{};
    if (!array(scoreboard,
               kTitleList,
               kTitleRowClass,
               kTitleRowStride,
               kTitleCapacity,
               titles,
               titleCount)) {
        return false;
    }
    for (std::size_t i = 0; i < titleCount; ++i) {
        std::uint32_t tag{}, key{};
        if (!read(scoreboard, titles + (i * kTitleRowStride) + kTitleTagOffset, tag)
            || !read(scoreboard, titles + (i * kTitleRowStride) + kTitleKeyOffset, key)) {
            return false;
        }
        if (keys.localizationTag == 0 || tag != keys.localizationTag) {
            continue;
        }
        for (std::size_t rank = 0; rank < placeKeys.size(); ++rank) {
            auto* slot = key == placeKeys[rank]  ? &result.placementTitles[rank]
                         : key == tiedKeys[rank] ? &result.tiedPlacementTitles[rank]
                                                 : nullptr;
            if (slot != nullptr) {
                if (*slot != -1) {
                    return false;
                }
                *slot = static_cast<std::int8_t>(i);
            }
        }
    }
    for (std::size_t rank = 0; keys.localizationTag != 0 && rank < placeKeys.size(); ++rank) {
        if (result.placementTitles[rank] == -1 || result.tiedPlacementTitles[rank] == -1) {
            return false;
        }
    }
    unsigned foundMode = 0;
    for (std::size_t i = 0; i < rowCount; ++i) {
        const auto row = rows + (i * kModeRowStride);
        std::uint32_t hash{};
        if (!read(settings, row + kModeKeyOffset, hash)) {
            return false;
        }
        if (hash != modeKey) {
            continue;
        }
        if (++foundMode != 1) {
            return false;
        }
        std::size_t values{}, count{};
        if (!array(settings,
                   row + kModeSettingList,
                   keys::kSettingReferenceClass,
                   keys::kSettingReferenceStride,
                   kModeSettingCapacity,
                   values,
                   count)) {
            return false;
        }
        unsigned found = 0;
        for (std::size_t j = 0; j < count; ++j) {
            std::size_t setting{};
            std::uint32_t cls{}, key{};
            std::int32_t number{};
            if (!relative(settings, values + (j * keys::kSettingReferenceStride), setting)
                || setting < keys::kSettingClassBefore
                || !read(settings, setting - keys::kSettingClassBefore, cls)
                || !read(settings, setting + keys::kSettingKeyOffset, key)
                || !read(settings, setting + keys::kSettingPayloadOffset, number)) {
                return false;
            }
            unsigned bit = 0;
            if (key == keys::kFinalBlowScoreKey) {
                bit = kFinalBlowSetting;
                result.finalBlowScore = number;
            }
            if (key == keys::kKillScoreKey) {
                bit = kKillSetting;
                result.killScore = number;
            }
            if (key == keys::kScoreToWinKey) {
                bit = kScoreLimitSetting;
                result.defaultScoreLimit = number;
            }
            if (key == keys::kIsFfaKey) {
                bit = kFfaSetting;
                result.isFfa = number;
            }
            if (key == keys::kRoundLimitKey) {
                bit = kRoundLimitSetting;
                result.roundLimit = number;
            }
            if (key == keys::kCompleteTimerKey) {
                bit = kCompleteSetting;
                result.completeSeconds = number;
            }
            if (key == keys::kPostgameTimerKey) {
                bit = kPostgameSetting;
                result.postgameSeconds = number;
            }
            if (key == keys::kLandingCinematicKey) {
                bit = kLandingSetting;
                result.landingCinematic = static_cast<std::uint32_t>(number);
            }
            if (key == keys::kTimeLimitKey) {
                bit = kTimeLimitSetting;
                result.timeLimitSeconds = number;
            }
            if (bit == 0) {
                continue;
            }
            const auto expectedClass = bit == kLandingSetting ? keys::kHashClass
                                       : bit == kFfaSetting   ? keys::kBoolClass
                                                              : keys::kIntegerClass;
            if ((found & bit) != 0 || cls != expectedClass) {
                return false;
            }
            found |= bit;
        }
        // Only Rumble authors the final blow score. Its absence stays visible to the mode, and a
        // row without a time limit has no installed clock.
        if ((found | kOptionalSettings) != kEverySetting) {
            return false;
        }
        result.hasFinalBlowScore = (found & kFinalBlowSetting) != 0;
        if ((found & kTimeLimitSetting) != 0 && result.timeLimitSeconds <= 0) {
            return false;
        }
    }
    if (foundMode != 1 || result.finalBlowScore < 0 || result.defaultScoreLimit <= 0
        || result.completeSeconds <= 0 || result.postgameSeconds < 0) {
        return false;
    }
    output = decoded;
    return true;
}

bool read_mode_settings(Bytes settings,
                        std::uint32_t modeKey,
                        std::span<SettingValue> values) noexcept {
    std::size_t rows{};
    std::size_t rowCount{};
    if (modeKey == 0 || modeKey == keys::kEmptyNameKey
        || !array(
            settings, kModeRows, kModeRowClass, kModeRowStride, kModeRowCapacity, rows, rowCount)) {
        return false;
    }
    for (auto& value : values) {
        value.found = false;
    }
    unsigned foundMode = 0;
    for (std::size_t i = 0; i < rowCount; ++i) {
        const auto row = rows + (i * kModeRowStride);
        std::uint32_t hash{};
        if (!read(settings, row + kModeKeyOffset, hash)) {
            return false;
        }
        if (hash != modeKey) {
            continue;
        }
        if (++foundMode != 1) {
            return false;
        }
        std::size_t list{};
        std::size_t count{};
        if (!array(settings,
                   row + kModeSettingList,
                   keys::kSettingReferenceClass,
                   keys::kSettingReferenceStride,
                   kModeSettingCapacity,
                   list,
                   count)) {
            return false;
        }
        for (std::size_t j = 0; j < count; ++j) {
            std::size_t setting{};
            std::uint32_t cls{};
            std::uint32_t key{};
            std::int32_t number{};
            if (!relative(settings, list + (j * keys::kSettingReferenceStride), setting)
                || setting < keys::kSettingClassBefore
                || !read(settings, setting - keys::kSettingClassBefore, cls)
                || !read(settings, setting + keys::kSettingKeyOffset, key)
                || !read(settings, setting + keys::kSettingPayloadOffset, number)) {
                return false;
            }
            for (auto& requested : values) {
                if (requested.key != key) {
                    continue;
                }
                if (requested.found) {
                    return false;
                }
                requested = {key, cls, number, true};
            }
        }
    }
    return foundMode == 1;
}

std::int32_t team_statistic_index(Bytes scoreboard, std::uint32_t statistic) noexcept {
    std::uint32_t count{};
    if (!read(scoreboard, scoreboards::kTeamStatisticCountOffset, count)
        || count > scoreboards::kTeamStatisticCapacity
        || scoreboard.size()
               < scoreboards::kTeamStatisticsOffset + (count * scoreboards::kStatisticStride)) {
        return -1;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t key{};
        if (!read(scoreboard,
                  scoreboards::kTeamStatisticsOffset + (i * scoreboards::kStatisticStride)
                      + scoreboards::kStatisticKeyOffset,
                  key)) {
            return -1;
        }
        if (key == statistic) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}
bool read_hud_title(Bytes scoreboard,
                    std::uint32_t localizationTag,
                    std::uint32_t key,
                    std::uint32_t argumentsTag,
                    std::int8_t& ordinal) noexcept {
    std::size_t rows{}, count{};
    if (!array(
            scoreboard, kTitleList, kTitleRowClass, kTitleRowStride, kTitleCapacity, rows, count)) {
        return false;
    }
    std::int8_t found = -1;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t tag{}, text{}, arguments{};
        const auto offset = rows + i * kTitleRowStride;
        if (!read(scoreboard, offset + kTitleTagOffset, tag)
            || !read(scoreboard, offset + kTitleKeyOffset, text)
            || !read(scoreboard, offset + kTitleArgumentsOffset, arguments)) {
            return false;
        }
        if (tag != localizationTag || text != key) {
            continue;
        }
        if (found >= 0 || arguments != argumentsTag) {
            return false;
        }
        found = static_cast<std::int8_t>(i);
    }
    ordinal = found;
    return true;
}
bool read_global_statistic(Bytes scoreboard,
                           std::uint32_t statistic,
                           std::uint32_t& count,
                           std::int32_t& index) noexcept {
    std::uint32_t size{};
    if (!read(scoreboard, scoreboards::kGlobalStatisticCountOffset, size)
        || size > scoreboards::kGlobalStatisticCapacity
        || scoreboard.size()
               < scoreboards::kGlobalStatisticsOffset + size * scoreboards::kStatisticStride) {
        return false;
    }
    std::int32_t found = -1;
    for (std::uint32_t i = 0; i < size; ++i) {
        std::uint32_t key{};
        if (!read(scoreboard,
                  scoreboards::kGlobalStatisticsOffset + i * scoreboards::kStatisticStride
                      + scoreboards::kStatisticKeyOffset,
                  key)) {
            return false;
        }
        if (key == statistic) {
            if (found >= 0) {
                return false;
            }
            found = static_cast<std::int32_t>(i);
        }
    }
    count = size;
    index = found;
    return true;
}
} // namespace sunrise::middleware::content::packages::tables::activity_rules

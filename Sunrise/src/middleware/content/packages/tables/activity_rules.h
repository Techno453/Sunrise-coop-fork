#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "activity_definition_reader.h"
#include "statistic_program.h"

namespace sunrise::middleware::content::packages::tables::activity_rules {

/**
 * Activity indices: the biased 12-bit index names 0 to 4094 (0xFFF is none), so the installed
 * table holds at most this many rows.
 */
inline constexpr std::size_t kActivityCapacity = kActivityDefinitionMaximumCount;
/** Placement titles a mode authors, for places and for ties. */
inline constexpr std::size_t kPlacementCapacity = 8;

/** Installed mode key of each activity, by activity index. */
struct ActivityModes final {
    std::array<std::uint32_t, kActivityCapacity> values{};
    std::size_t count{};
};

struct Settings final {
    /** `base_final_blow_score`; valid only when `hasFinalBlowScore`. */
    std::int32_t finalBlowScore{};
    std::int32_t killScore{};
    std::int32_t defaultScoreLimit{};
    std::int32_t completeSeconds{};
    std::int32_t postgameSeconds{};
    std::int32_t isFfa{};
    std::int32_t roundLimit{};
    std::uint32_t landingCinematic{};
    std::uint32_t playerStatisticCount{};
    std::uint32_t teamStatisticCount{};
    std::array<std::int8_t, kPlacementCapacity> placementTitles{};
    std::array<std::int8_t, kPlacementCapacity> tiedPlacementTitles{};
    /** Rumble authors `base_final_blow_score`; the team kill modes author only the kill score. */
    bool hasFinalBlowScore{};
    /**
     * `time_limit` of a single-round mode: the match seconds a private match plays when no time
     * option is selected. Zero when the row authors none.
     */
    std::int32_t timeLimitSeconds{};
    /** Zone modes only: `score_control_zone_captured`, the team award for a captured zone. */
    std::int32_t zoneCaptureScore{};
    /** Zone modes only: `score_control_zone_neutralized`, the award for a neutralized zone. */
    std::int32_t zoneNeutralizeScore{};
    /** Zone modes only: kill bonus while the killer's team holds the zone advantage. */
    std::int32_t advantageKillBonus{};
    /** Zone modes only: kill bonus while the killer's team holds every zone (power play). */
    std::int32_t powerPlayKillBonus{};
};

/** One requested typed setting of a mode row. */
struct SettingValue final {
    std::uint32_t key{};
    std::uint32_t contentClass{};
    std::int32_t value{};
    bool found{};
};

struct ModeData final {
    Settings settings{};
    statistic_program::Catalog statistics{};
};

/**
 * Localized keys belong to the mode definition, not the package decoder. A zero localization tag
 * requests no placement titles; every title ordinal then stays -1.
 */
struct PlacementKeys final {
    std::uint32_t localizationTag{};
    std::array<std::uint32_t, kPlacementCapacity> places{};
    std::array<std::uint32_t, kPlacementCapacity> ties{};
};

/** Decode each activity's installed mode key independently of which modes have implementations. */
[[nodiscard]] bool parse_activity_modes(std::span<const std::byte> activities,
                                        std::span<const std::byte> modes,
                                        ActivityModes& output) noexcept;

/**
 * Reads the statistic list counts of one scoreboard config; `score` must lead both lists.
 * @return False when a count is zero or above its capacity, or a list runs past the config.
 */
[[nodiscard]] bool read_statistic_counts(std::span<const std::byte> scoreboard,
                                         std::uint32_t& players,
                                         std::uint32_t& teams) noexcept;

/** Shared installed score-settings format. The mode validates its supported scoring semantics. */
[[nodiscard]] bool parse_mode(std::span<const std::byte> settings,
                              std::span<const std::byte> scoreboard,
                              std::uint32_t modeKey,
                              const PlacementKeys& keys,
                              ModeData& output) noexcept;

/**
 * Reads the requested keys of one installed mode row. A key present twice, a mode row present
 * twice or absent, or a malformed value list rejects the read; absent keys stay not found.
 */
[[nodiscard]] bool read_mode_settings(std::span<const std::byte> settings,
                                      std::uint32_t modeKey,
                                      std::span<SettingValue> values) noexcept;

/** Position of one statistic in the scoreboard config's team statistic list, or -1. */
[[nodiscard]] std::int32_t team_statistic_index(std::span<const std::byte> scoreboard,
                                                std::uint32_t statistic) noexcept;
/** Validate the separate global-statistic list and locate a unique named column. */
[[nodiscard]] bool read_global_statistic(std::span<const std::byte> scoreboard,
                                         std::uint32_t statistic,
                                         std::uint32_t& count,
                                         std::int32_t& index) noexcept;
/** Validate the title list and locate a unique installed text/argument binding; -1 if absent. */
[[nodiscard]] bool read_hud_title(std::span<const std::byte> scoreboard,
                                  std::uint32_t localizationTag,
                                  std::uint32_t key,
                                  std::uint32_t argumentsTag,
                                  std::int8_t& ordinal) noexcept;

} // namespace sunrise::middleware::content::packages::tables::activity_rules

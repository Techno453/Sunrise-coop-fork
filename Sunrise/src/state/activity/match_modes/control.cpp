#include "control.h"

#include <array>

#include "../../build_data/activity_rules/runtime.h"
#include "lifecycle.h"

namespace sunrise::state::activity::match_modes::control {
namespace {
namespace incident = middleware::bap::activity_message::incident;
namespace parsed = middleware::content::packages::tables::activity_rules;
namespace keys = middleware::content::packages::tables::activity_settings;

/** Scoreboard config (`C80809a12`) named by the Control settings row's `unk58`. */
constexpr std::uint32_t kScoreboard = 0x80BA0002U;
/**
 * The controller's `placements_capture_zones.zones[0..2]` containers name type-4 object slots 13,
 * 14 and 15, each carrying the capture component.
 */
constexpr std::uint16_t kZoneA = 13;
constexpr std::uint16_t kZoneB = 14;
constexpr std::uint16_t kZoneC = 15;
/** Zones in `kZones`. */
constexpr std::uint8_t kZoneCount = 3;
/** Incident targets 1348..1350 `pvp_control_zone_captured_a/b/c`: the type-6 per-zone captures. */
constexpr std::uint32_t kZoneACaptured = 0x30323E15U;
constexpr std::uint32_t kZoneBCaptured = 0x30323E16U;
constexpr std::uint32_t kZoneCCaptured = 0x30323E17U;
/** The three authored capture zones, each named by its capture event. */
constexpr Zones kZones{{{{lifecycle::kControllerGroup, kZoneA, kZoneACaptured},
                         {lifecycle::kControllerGroup, kZoneB, kZoneBCaptured},
                         {lifecycle::kControllerGroup, kZoneC, kZoneCCaptured}}},
                       kZoneCount};
/** Incident target 3281 `pvp_score_control_advantage_kill`: the type-25 advantage kill award. */
constexpr std::uint32_t kAdvantageKillNotice = 0x71A7D4E1U;
/** Incident target 2970 `pvp_score_control_power_play_kill`: the type-25 power play kill award. */
constexpr std::uint32_t kPowerPlayKillNotice = 0x66CDE8F6U;
/** Incident targets 5655..5653 `pvp_score_control_zone_captured_a/b/c`: the type-25 awards. */
constexpr std::array<std::uint32_t, kZoneCount> kZoneCapturedNotice{
    0xBCB448B2U, 0xBCB448B1U, 0xBCB448B0U};

/** @return True for the single-round, three-zone team shape every Control row shares. */
bool validate(const parsed::ModeData& data) noexcept {
    const auto& rules = data.settings;
    for (std::size_t i = 0; i < rules.placementTitles.size(); ++i) {
        if (rules.placementTitles[i] != -1 || rules.tiedPlacementTitles[i] != -1) {
            return false;
        }
    }
    return rules.isFfa == 0 && rules.roundLimit == lifecycle::kSingleRound && rules.killScore > 0
           && !rules.hasFinalBlowScore && rules.defaultScoreLimit > 0 && rules.completeSeconds > 0
           && rules.postgameSeconds >= 0 && rules.landingCinematic == lifecycle::kTeamLanding
           && rules.zoneCaptureScore >= 0 && rules.zoneNeutralizeScore >= 0
           && rules.advantageKillBonus >= 0 && rules.powerPlayKillBonus >= 0;
}

/** The zone scores and kill bonuses a Control row adds, in `load` order. */
enum ZoneSetting : std::size_t {
    zoneCaptureScore,
    zoneNeutralizeScore,
    advantageKillBonus,
    powerPlayKillBonus,
    zoneSettingCount,
};

/**
 * Loads the installed row of `Mode` with its zone scores and kill bonuses; its team scoreboard
 * authors no placement titles to bind.
 */
template <std::uint32_t Mode>
bool load(std::span<const std::byte> settings,
          std::span<const std::byte> scoreboard,
          parsed::ModeData& output) noexcept {
    constexpr parsed::PlacementKeys kNoTitles{};
    parsed::ModeData data{};
    std::array<parsed::SettingValue, zoneSettingCount> zoneScores{
        {{keys::kZoneCaptureScoreKey, 0, 0, false},
         {keys::kZoneNeutralizeScoreKey, 0, 0, false},
         {keys::kAdvantageKillBonusKey, 0, 0, false},
         {keys::kPowerPlayKillBonusKey, 0, 0, false}}};
    if (!parsed::parse_mode(settings, scoreboard, Mode, kNoTitles, data)
        || !parsed::read_mode_settings(settings, Mode, zoneScores)) {
        return false;
    }
    for (const auto& score : zoneScores) {
        if (!score.found || score.contentClass != keys::kIntegerClass) {
            return false;
        }
    }
    data.settings.zoneCaptureScore = zoneScores[zoneCaptureScore].value;
    data.settings.zoneNeutralizeScore = zoneScores[zoneNeutralizeScore].value;
    data.settings.advantageKillBonus = zoneScores[advantageKillBonus].value;
    data.settings.powerPlayKillBonus = zoneScores[powerPlayKillBonus].value;
    if (!validate(data)) {
        return false;
    }
    output = data;
    return true;
}

using Notice = match_state::script_policy::Notice;
constexpr std::array kNotices{NoticeBinding{Notice::killScore, lifecycle::kKillScoreNotice},
                              NoticeBinding{Notice::advantageKill, kAdvantageKillNotice},
                              NoticeBinding{Notice::powerPlayKill, kPowerPlayKillNotice},
                              NoticeBinding{Notice::zoneA, kZoneCapturedNotice[0]},
                              NoticeBinding{Notice::zoneB, kZoneCapturedNotice[1]},
                              NoticeBinding{Notice::zoneC, kZoneCapturedNotice[2]}};

/** Authored native objects and notices; Lua chooses awards, lifecycle and enablement. */
template <std::uint32_t Mode, std::uint32_t Scoreboard = kScoreboard>
Definition control_mode(std::string_view name) noexcept {
    Definition value{
        .key = Mode,
        .name = name,
        .settingsTag = lifecycle::kSettings,
        .scoreboardTag = Scoreboard,
        .qualifyingKillKey = lifecycle::kQualifyingKill,
        .scoreNoticeKey = lifecycle::kKillScoreNotice,
        .phases = lifecycle::kPhases,
        .zones = kZones,
        .load = &load<Mode>,
        .validate = &validate,
        .objectiveHud = objective_hud::kControl,
        // Native Control descriptor +2B4 has two teams; catalog factions 17/18 name them.
        .teamCount = 2,
        .scripted = true,
        .scriptedKillEligible = &lifecycle::scores_kill,
        .scriptedIntroBindings = lifecycle::kTeamIntroBindings,
        .scriptedNoticeBindings = kNotices};
    return value;
}
} // namespace

const Definition& definition() noexcept {
    static const Definition mode = control_mode<kMode>("pvp_control");
    return mode;
}

} // namespace sunrise::state::activity::match_modes::control

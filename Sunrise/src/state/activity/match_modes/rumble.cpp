#include "rumble.h"

#include "../../../middleware/bap/activity_message/scoreboard_record.h"
#include "../../build_data/activity_rules/runtime.h"
#include "../definition.h"
#include "../match_state/definition.h"
#include "lifecycle.h"

namespace sunrise::state::activity::match_modes::rumble {
namespace {
namespace parsed = middleware::content::packages::tables::activity_rules;
using ScriptClip = match_state::script_policy::IntroClip;

/** Content lookup only. Its order has no effect on playback. */
constexpr std::array kIntroBindings{
    IntroBinding{ScriptClip::ffaLanding, {lifecycle::kControllerGroup, lifecycle::kFadeLanding}},
    IntroBinding{ScriptClip::matchTitle, {lifecycle::kControllerGroup, lifecycle::kTitleLanding}},
    IntroBinding{ScriptClip::openingC, {lifecycle::kControllerGroup, lifecycle::kIntroThird}},
    IntroBinding{ScriptClip::openingB, {lifecycle::kControllerGroup, lifecycle::kIntroSecond}},
    IntroBinding{ScriptClip::openingA, {lifecycle::kControllerGroup, lifecycle::kIntroFirst}}};

/** Free-for-all scoreboard config named by the Rumble settings row's `unk58`. */
constexpr std::uint32_t kScoreboard = 0x80BA0015U;
/** Largest title index the one-biased 6-bit scoreboard title field carries. */
constexpr std::int8_t kLastTitle = 62;
/** String table `0x80B9E378` that holds the free-for-all scoreboard titles. */
constexpr std::uint32_t kTitleTable = 0x80B9E378U;

/**
 * The free-for-all scoreboard's title keys in `kTitleTable`: first to eighth place, then tied
 * first to tied eighth.
 */
constexpr parsed::PlacementKeys kPlacementTitles{kTitleTable,
                                                 {0x6355CF95,
                                                  0x505E00D9,
                                                  0x6A5A5870,
                                                  0x5A628DC1,
                                                  0x546045C8,
                                                  0x66671DD3,
                                                  0x6864E222,
                                                  0x5A4478FD},
                                                 {0xC1AA55CA,
                                                  0xA5D0ED26,
                                                  0xFA449BBD,
                                                  0x06B7653E,
                                                  0x3F9EEDA5,
                                                  0x24F86F98,
                                                  0x7902227F,
                                                  0x5FF9E882}};

/** @return True when every place and tie names an authored scoreboard title. */
bool placements_valid(const parsed::Settings& rules) noexcept {
    for (std::size_t i = 0; i < rules.placementTitles.size(); ++i) {
        if (rules.placementTitles[i] < 0 || rules.placementTitles[i] > kLastTitle
            || rules.tiedPlacementTitles[i] < 0 || rules.tiedPlacementTitles[i] > kLastTitle) {
            return false;
        }
    }
    return true;
}

/** @return True for the single-round free-for-all lifecycle Rumble uses. */
bool free_for_all(const parsed::Settings& rules) noexcept {
    return placements_valid(rules) && rules.isFfa == 1
           && rules.roundLimit == lifecycle::kSingleRound && rules.defaultScoreLimit > 0
           && rules.completeSeconds > 0 && rules.postgameSeconds >= 0
           && rules.landingCinematic == lifecycle::kTeamLanding;
}

/** Rumble scores the installed final blow score and authors no kill score. */
bool validate(const parsed::ModeData& data) noexcept {
    const auto& rules = data.settings;
    return free_for_all(rules) && rules.killScore == 0 && rules.hasFinalBlowScore
           && rules.finalBlowScore >= 0;
}

/** Loads the installed row of `Mode` with the free-for-all placement titles. */
template <std::uint32_t Mode, bool (*Validate)(const parsed::ModeData&) noexcept>
bool load(std::span<const std::byte> settings,
          std::span<const std::byte> scoreboard,
          parsed::ModeData& output) noexcept {
    parsed::ModeData data{};
    if (!parsed::parse_mode(settings, scoreboard, Mode, kPlacementTitles, data)
        || !Validate(data)) {
        return false;
    }
    output = data;
    return true;
}

} // namespace

const Definition& definition() noexcept {
    static const Definition mode{.key = kMode,
                                 .name = "pvp_rumble",
                                 .settingsTag = lifecycle::kSettings,
                                 .scoreboardTag = kScoreboard,
                                 .qualifyingKillKey = lifecycle::kQualifyingKill,
                                 .scoreNoticeKey = lifecycle::kFinalBlowNotice,
                                 .phases = lifecycle::kPhases,
                                 .load = &load<kMode, &validate>,
                                 .validate = &validate,
                                 .scripted = true,
                                 .scriptedKillEligible = &lifecycle::scores_kill,
                                 .scriptedIntroBindings = kIntroBindings};
    return mode;
}

} // namespace sunrise::state::activity::match_modes::rumble

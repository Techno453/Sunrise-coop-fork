#pragma once

#include "../../../middleware/bap/activity_message/scoreboard_record.h"
#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "../../build_data/activity_rules/runtime.h"
#include "../definition.h"
#include "../match_state/definition.h"
#include "definition.h"

/** Match lifecycle pieces shared by the single-round score modes. */
namespace sunrise::state::activity::match_modes::lifecycle {

/**
 * Common PvP controller group. Its slots carry the authored intro and landing cinematics, the
 * capture objectives.
 */
inline constexpr std::uint32_t kControllerGroup = 0x39774EE1U;
/** Intro cinematic components `815B9897`, `815B989A` and `815B989D`. */
inline constexpr std::uint16_t kIntroFirst = 5;
inline constexpr std::uint16_t kIntroSecond = 6;
inline constexpr std::uint16_t kIntroThird = 7;
/** Default landing `9085DFB5`, whose timeline raises the native `pvp_intro` title. */
inline constexpr std::uint16_t kTitleLanding = 8;
/** Landing `815B98A6` (`landing_pvp_ffa`), whose fade track clears the world transition. */
inline constexpr std::uint16_t kFadeLanding = 10;
/** FNV-1 of `team`, the installed `pvp_landing_cinematic` value these commands answer. */
inline constexpr std::uint32_t kTeamLanding = 0xCA2C21CAU;

/** Installed mode settings table every mode reads its row from. */
inline constexpr std::uint32_t kSettings =
    middleware::content::packages::tables::activity_settings::kTableTag;
/** Incident target 6089 `kill.pvp`: the type-1 kill whose native report carries the facts. */
inline constexpr std::uint32_t kQualifyingKill = 0xCB08AEADU;
/** Incident target 2769 `pvp_score_final_blow`: the award paired with `base_final_blow_score`. */
inline constexpr std::uint32_t kFinalBlowNotice = 0x5FDAD026U;
/** Incident target 5268 `pvp_score_kill`: the type-25 award paired with `base_kill_score`. */
inline constexpr std::uint32_t kKillScoreNotice = 0xB00DB2B1U;
/** One scoring round: the installed `pvp_round_limit` of every single-round mode. */
inline constexpr std::int32_t kSingleRound = 1;

/** Available native team-mode clips. The script chooses sequence and timing. */
inline constexpr std::array kTeamIntroBindings{
    IntroBinding{match_state::script_policy::IntroClip::openingA, {kControllerGroup, kIntroFirst}},
    IntroBinding{match_state::script_policy::IntroClip::openingB, {kControllerGroup, kIntroSecond}},
    IntroBinding{match_state::script_policy::IntroClip::openingC, {kControllerGroup, kIntroThird}},
    IntroBinding{match_state::script_policy::IntroClip::matchTitle,
                 {kControllerGroup, kTitleLanding}},
    IntroBinding{match_state::script_policy::IntroClip::ffaLanding,
                 {kControllerGroup, kFadeLanding}}};

/** Native lifetime values: active play, completion, postgame, advance and the intro. */
inline constexpr PhaseIds kPhases{match_state::kActive,
                                  match_state::kComplete,
                                  match_state::kPostgame,
                                  match_state::kAdvance,
                                  match_state::kIntro};

/** @return True when the kill of another partition's player scores. */
[[nodiscard]] inline bool scores_kill(const KillContext& context) noexcept {
    const auto& facts = context.facts;
    return facts.ordinaryVictim
           && facts.reason != middleware::bap::activity_message::incident::kExcludedKillReason
           && facts.killingPlayer != facts.deadPlayer && context.killerPartition >= 0
           && context.victimPartition >= 0 && context.killerPartition != context.victimPartition;
}

} // namespace sunrise::state::activity::match_modes::lifecycle

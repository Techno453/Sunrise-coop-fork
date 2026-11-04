#pragma once

#include <cstddef>
#include <cstdint>

#include "../reader/layout.h"
#include "../schema/generated/tag_classes_80804e.h"
#include "../schema/generated/tag_classes_808090.h"
#include "../schema/generated/tag_classes_808092.h"

/**
 * Keys and value classes of the installed mode settings table. A key is the FNV-1 hash of the
 * setting name; a key whose name is unknown is named after its consumer.
 */
namespace sunrise::middleware::content::packages::tables::activity_settings {

namespace generated = schema::generated;

/** FNV-1 of the empty name. No installed mode, group or row key hashes to it. */
inline constexpr std::uint32_t kEmptyNameKey = 0x811C9DC5U;

/** Installed mode settings table, class `kTableClass`, holding one row per mode key. */
inline constexpr std::uint32_t kTableTag = 0x80BEA325U;
inline constexpr std::uint32_t kTableClass = generated::C80804e66::kClass;

/**
 * A setting list (element class `kSettingReferenceClass`) holds self-relative pointers to typed
 * values. A value's class precedes it; the value is its key, then its 32-bit payload.
 */
inline constexpr std::uint32_t kSettingReferenceClass = generated::C80809289::kClass;
inline constexpr std::size_t kSettingReferenceStride = sizeof(generated::C80809289);
inline constexpr std::size_t kSettingClassBefore = reader::layout::kClassWordSize;

/** Typed value classes of one setting. */
inline constexpr std::uint32_t kHashClass = generated::C808090c5::kClass;
inline constexpr std::uint32_t kFloatClass = generated::C808090c6::kClass;
inline constexpr std::uint32_t kIntegerClass = generated::C808090c7::kClass;
inline constexpr std::uint32_t kBoolClass = generated::C808090c8::kClass;
/** Every typed value starts with its key. */
inline constexpr std::size_t kSettingKeyOffset = offsetof(generated::C808090c5, unk0);
// Tag layout: the payload follows the key; the integer and bool layouts leave it unnamed.
inline constexpr std::size_t kSettingPayloadOffset = offsetof(generated::C808090c5, unk4);
static_assert(offsetof(generated::C808090c6, unk4) == kSettingPayloadOffset
              && offsetof(generated::C808090c7, gap4) == kSettingPayloadOffset
              && offsetof(generated::C808090c8, gap4) == kSettingPayloadOffset);

/** `base_final_blow_score`: Rumble's award for a final blow. */
inline constexpr std::uint32_t kFinalBlowScoreKey = 0xAA7DB325U;
/** `base_kill_score`: the award for a kill of another player. */
inline constexpr std::uint32_t kKillScoreKey = 0x18194EAEU;
/** `score_to_win`: the match score limit, or the won rounds a round mode needs (HUD FD96F0). */
inline constexpr std::uint32_t kScoreToWinKey = 0xBE4DB480U;
/** `time_limit`: seconds of match time, or of one round in a round mode. */
inline constexpr std::uint32_t kTimeLimitKey = 0xF5EC68CAU;
/** `is_ffa`: free-for-all placements instead of teams. */
inline constexpr std::uint32_t kIsFfaKey = 0xDBDFF5B1U;
/** `pvp_round_limit`: the most rounds a match plays; -1 is no cap. */
inline constexpr std::uint32_t kRoundLimitKey = 0x21D29830U;
/** `activity_complete_timer`: seconds of the completion phase. */
inline constexpr std::uint32_t kCompleteTimerKey = 0x2D5BB36EU;
/** `activity_postgame_timer`: seconds of the postgame phase. */
inline constexpr std::uint32_t kPostgameTimerKey = 0x295EEA83U;
/** `pvp_landing_cinematic`: the FNV-1 name of the landing the intro ends on. */
inline constexpr std::uint32_t kLandingCinematicKey = 0xE12E49B7U;
/** `use_round_scoring`: native FDA0C0 leads the team rows with the round column. */
inline constexpr std::uint32_t kRoundScoringKey = 0xF36E9879U;
/** `use_team_scoring`. */
inline constexpr std::uint32_t kTeamScoringKey = 0xE8005B14U;
/** Name unknown: native DC8CE0 selects the 12E9330 respawn timers at death. */
inline constexpr std::uint32_t kDeathTimersKey = 0x62DA8B8AU;
/** Name unknown: the respawn delay 12E9330 copies to player `+0x40`. */
inline constexpr std::uint32_t kRespawnDelayKey = 0x1F8692DEU;
/** `score_control_zone_captured`: the team award for a captured zone. */
inline constexpr std::uint32_t kZoneCaptureScoreKey = 0x0D02D5D1U;
/** `score_control_zone_neutralized`: the award for a neutralized zone. */
inline constexpr std::uint32_t kZoneNeutralizeScoreKey = 0xCF31C47AU;
/**
 * Control integer settings, names unknown. Control and Iron Banner Control author 1 and 2 and
 * Momentum Control 2 and 4: the advantage and power play kill bonuses that, over the base kill
 * score of 1, make a Control kill worth 1, 2 or 3 points.
 */
inline constexpr std::uint32_t kAdvantageKillBonusKey = 0x30B4FF85U;
inline constexpr std::uint32_t kPowerPlayKillBonusKey = 0xEFBD3140U;

} // namespace sunrise::middleware::content::packages::tables::activity_settings

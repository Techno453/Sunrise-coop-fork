#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "../../../middleware/bap/activity_message/cinematic_record.h"
#include "../../../middleware/bap/activity_message/kill_incident_codec.h"
#include "../../../middleware/content/packages/tables/activity_rules.h"
#include "../match_state/script_policy.h"

namespace sunrise::state::activity::match_modes {

struct IntroClip final {
    std::uint32_t group{};
    std::uint16_t slot{};
};

/** Native content identity for a symbolic script request; the script chooses the sequence. */
struct IntroBinding final {
    match_state::script_policy::IntroClip name{};
    IntroClip clip{};
};

struct PhaseIds final {
    std::uint8_t active{};
    std::uint8_t completion{};
    std::uint8_t postgame{};
    std::uint8_t advance{};
    std::uint8_t intro{};
};

struct KillContext final {
    const middleware::bap::activity_message::incident::KillFacts& facts;
    std::int8_t killerPartition{};
    std::int8_t victimPartition{};
    /** Keys of the incident targets the reporting client selected for this kill. */
    std::span<const std::uint32_t> classification{};
    /** Capture zones the killer's partition holds when the kill is accepted. */
    std::uint8_t killerZones{};
};

/**
 * One authored capture zone placement: a template group, its type-4 object slot and the key of
 * the type-6 incident target whose capture event names this zone.
 */
struct ZonePlacement final {
    std::uint32_t group{};
    std::uint16_t slot{};
    std::uint32_t captureKey{};
};

/**
 * FNV-1 names of the objective HUDs 0xBEA020 selects from the lifetime's `pvp_objective_hud`
 * setting (its compare values at 0x1FB7954..0x1FB7987). A mode names the HUD its row authors
 * among its own name settings, or the one retail shows it.
 */
namespace objective_hud {
/** No objective HUD (mode 0). */
inline constexpr std::uint32_t kNone = 0;
/** "control": the zone HUD (mode 1, 0xBEAF30), lettering each zone from component `+0x6C`. */
inline constexpr std::uint32_t kControl = 0xBEAE5E70U;
} // namespace objective_hud

/** Authoring capacity of a mode's zone placements, not a wire count. */
inline constexpr std::size_t kZonePlacementCapacity = 3;

/** The capture zones whose object Auth the host publishes for one mode. */
struct Zones final {
    std::array<ZonePlacement, kZonePlacementCapacity> placements{};
    std::uint8_t count{};
};

struct TimeDecision final {
    bool finish{};
    std::uint64_t endedAtTicks{};
};

struct NoticeBinding final {
    match_state::script_policy::Notice name{};
    std::uint32_t key{};
};

/** Native content bindings and qualification; Lua owns match decisions. */
struct Definition final {
    std::uint32_t key{};
    std::string_view name{};
    std::uint32_t settingsTag{};
    std::uint32_t scoreboardTag{};
    std::uint32_t qualifyingKillKey{};
    std::uint32_t scoreNoticeKey{};
    PhaseIds phases{};
    /** Capture objects available to the Lua policy. */
    Zones zones{};
    bool (*load)(
        std::span<const std::byte> settings,
        std::span<const std::byte> scoreboard,
        middleware::content::packages::tables::activity_rules::ModeData& output) noexcept {};
    bool (*validate)(
        const middleware::content::packages::tables::activity_rules::ModeData&) noexcept {};
    /** The objective HUD the lifetime names for the mode, from `objective_hud`. */
    std::uint32_t objectiveHud{objective_hud::kNone};
    /** Configured team partitions, including empty teams; zero keeps membership enumeration. */
    std::uint8_t teamCount{};
    /** Lua owns every match policy callback; the registry rejects mixed ownership. */
    bool scripted{};
    /** Native kill qualification for scripted modes. */
    bool (*scriptedKillEligible)(const KillContext&) noexcept {};
    std::span<const IntroBinding> scriptedIntroBindings{};
    /** Nonempty catalogs require every kill decision to select a notice or explicitly none. */
    std::span<const NoticeBinding> scriptedNoticeBindings{};
};

/** Explicit support registry. Finding an installed mode does not enable an implementation. */
[[nodiscard]] std::span<const Definition* const> supported() noexcept;
[[nodiscard]] const Definition* find(std::uint32_t key) noexcept;
[[nodiscard]] bool valid(const Definition& mode) noexcept;

} // namespace sunrise::state::activity::match_modes

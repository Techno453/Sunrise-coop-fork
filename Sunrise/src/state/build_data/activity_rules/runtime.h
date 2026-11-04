#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "../../../middleware/content/packages/tables/activity_rules.h"
#include "../../../middleware/content/packages/tables/incident_targets.h"
#include "../../../middleware/content/packages/tables/statistic_program.h"
#include "../../activity/match_modes/definition.h"

namespace sunrise::state::build_data::activity_rules {

/** Activities one published catalog maps to modes: one per installed activity row. */
inline constexpr std::size_t kActivityCapacity =
    middleware::content::packages::tables::activity_rules::kActivityCapacity;
/** Registered mode definitions one published catalog holds. */
inline constexpr std::size_t kModeCapacity = 2;

/** One activity's mode: its definition, installed settings, programs and incident targets. */
struct Rules final {
    middleware::content::packages::tables::activity_rules::Settings settings{};
    const activity::match_modes::Definition* mode{};
    /** Immutable installed programs; storage belongs to this catalog, not a recipient. */
    const middleware::content::packages::tables::statistic_program::Catalog* statistics{};
    const middleware::content::packages::tables::incident_targets::Catalog* incidentTargets{};
    /** Incident target index of the mode's `scoreNoticeKey`, the default kill score notice. */
    std::uint16_t scoreNoticeTarget{};
};

/** Definition storage must outlive the catalog; parsed data is copied at publication. */
struct LoadedMode final {
    const activity::match_modes::Definition* definition{};
    const middleware::content::packages::tables::activity_rules::ModeData* data{};
};

/** Atomically publish every validated mode once. Failed publication changes nothing. */
[[nodiscard]] bool
publish(std::span<const std::uint32_t> activities,
        std::span<const LoadedMode> modes,
        const middleware::content::packages::tables::incident_targets::Catalog& targets) noexcept;
[[nodiscard]] bool resolve(std::int16_t activityIndex, Rules& output) noexcept;
/** The installed mode key of one activity, whether or not a definition supports it. */
[[nodiscard]] bool installed_mode(std::int16_t activityIndex, std::uint32_t& key) noexcept;

} // namespace sunrise::state::build_data::activity_rules

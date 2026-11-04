#include <Windows.h>

#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "runtime.h"

namespace sunrise::state::build_data::activity_rules {
namespace {
namespace parsed = middleware::content::packages::tables::activity_rules;
namespace programs = middleware::content::packages::tables::statistic_program;
namespace targets = middleware::content::packages::tables::incident_targets;
SRWLOCK g_lock = SRWLOCK_INIT;
std::array<std::uint32_t, kActivityCapacity> g_activityModes{};
std::size_t g_activityCount{};
std::array<Rules, kModeCapacity> g_rules{};
std::array<programs::Catalog, kModeCapacity> g_statistics{};
std::size_t g_modeCount{};
middleware::content::packages::tables::incident_targets::Catalog g_incidentTargets{};

bool valid_data(const parsed::ModeData& data) noexcept {
    const auto& settings = data.settings;
    const auto& statistics = data.statistics;
    if (settings.playerStatisticCount == 0
        || settings.playerStatisticCount > programs::kPlayerStatisticCapacity
        || settings.teamStatisticCount == 0
        || settings.teamStatisticCount > programs::kTeamStatisticCapacity
        || statistics.playerColumnCount != settings.playerStatisticCount
        || statistics.teamColumnCount != settings.teamStatisticCount
        || statistics.playerColumns[0] != programs::kScoreStatistic
        || statistics.teamColumns[0] != programs::kScoreStatistic
        || statistics.eventCount > statistics.events.size()
        || statistics.computedCount > statistics.computed.size()) {
        return false;
    }
    for (std::size_t i = 0; i < statistics.eventCount; ++i) {
        if (!programs::valid(statistics.events[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < statistics.computedCount; ++i) {
        if (!programs::valid(statistics.computed[i])) {
            return false;
        }
    }
    return true;
}

bool lookup(std::uint32_t key, Rules& output) noexcept {
    for (std::size_t i = 0; i < g_modeCount; ++i) {
        if (g_rules[i].mode->key == key) {
            output = g_rules[i];
            return true;
        }
    }
    return false;
}
} // namespace

bool publish(
    std::span<const std::uint32_t> activities,
    std::span<const LoadedMode> modes,
    const middleware::content::packages::tables::incident_targets::Catalog& targets) noexcept {
    if (targets.targetCount != targets::kInstalledTargetCount) {
        core::log::writef(core::log::Channel::state,
                          core::log::Level::warn,
                          "ev=activity_rules stage=publish result=refused reason=target_count "
                          "count=%u expected=%zu",
                          static_cast<unsigned>(targets.targetCount),
                          targets::kInstalledTargetCount);
        return false;
    }
    if (activities.empty() || activities.size() > kActivityCapacity || modes.empty()
        || modes.size() > kModeCapacity || targets.groupCount == 0
        || targets.groupCount > targets.groups.size() || targets.nodeCount > targets.nodes.size()) {
        return false;
    }
    std::array<Rules, kModeCapacity> staged{};
    for (std::size_t i = 0; i < modes.size(); ++i) {
        const auto& input = modes[i];
        if (input.definition == nullptr || input.data == nullptr
            || !activity::match_modes::valid(*input.definition) || !valid_data(*input.data)
            || !input.definition->validate(*input.data)) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (modes[j].definition->key == input.definition->key) {
                return false;
            }
        }
        auto& rules = staged[i];
        rules.settings = input.data->settings;
        rules.mode = input.definition;
        bool notice = false, kill = false;
        for (std::uint16_t n = 0; n < targets.targetCount; ++n) {
            const auto& target = targets.targets[n];
            if (target.key == rules.mode->qualifyingKillKey) {
                if (kill || target.type != targets::kKillType) {
                    return false;
                }
                kill = true;
            }
            if (target.key == rules.mode->scoreNoticeKey) {
                if (notice || target.type != targets::kAmountType) {
                    return false;
                }
                rules.scoreNoticeTarget = n;
                notice = true;
            }
        }
        if (!notice || !kill) {
            return false;
        }
        for (const auto& binding : rules.mode->scriptedNoticeBindings) {
            unsigned matches{};
            for (std::uint16_t n = 0; n < targets.targetCount; ++n) {
                if (targets.targets[n].key != binding.key) {
                    continue;
                }
                if (targets.targets[n].type != targets::kAmountType) {
                    return false;
                }
                ++matches;
            }
            if (matches != 1) {
                return false;
            }
        }
    }
    AcquireSRWLockExclusive(&g_lock);
    const bool fresh = g_activityCount == 0;
    if (fresh) {
        for (std::size_t i = 0; i < activities.size(); ++i) {
            g_activityModes[i] = activities[i];
        }
        g_incidentTargets = targets;
        for (std::size_t i = 0; i < modes.size(); ++i) {
            g_statistics[i] = modes[i].data->statistics;
            g_rules[i] = staged[i];
            g_rules[i].statistics = &g_statistics[i];
            g_rules[i].incidentTargets = &g_incidentTargets;
        }
        g_modeCount = modes.size();
        g_activityCount = activities.size();
    }
    ReleaseSRWLockExclusive(&g_lock);
    return fresh;
}

bool resolve(std::int16_t activityIndex, Rules& output) noexcept {
    if (activityIndex < 0) {
        return false;
    }
    AcquireSRWLockShared(&g_lock);
    const auto index = static_cast<std::size_t>(activityIndex);
    const bool found = index < g_activityCount && lookup(g_activityModes[index], output);
    ReleaseSRWLockShared(&g_lock);
    return found;
}

bool installed_mode(std::int16_t activityIndex, std::uint32_t& key) noexcept {
    if (activityIndex < 0) {
        return false;
    }
    AcquireSRWLockShared(&g_lock);
    const auto index = static_cast<std::size_t>(activityIndex);
    const bool found =
        index < g_activityCount && g_activityModes[index] != 0
        && g_activityModes[index]
               != middleware::content::packages::tables::activity_settings::kEmptyNameKey;
    if (found) {
        key = g_activityModes[index];
    }
    ReleaseSRWLockShared(&g_lock);
    return found;
}

} // namespace sunrise::state::build_data::activity_rules

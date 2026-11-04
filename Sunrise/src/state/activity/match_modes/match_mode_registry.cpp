#include "../../../middleware/bap/activity_message/auth_fields.h"
#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "../match_state/definition.h"
#include "control.h"
#include "definition.h"
#include "rumble.h"

namespace sunrise::state::activity::match_modes {
namespace {
namespace auth = middleware::bap::activity_message::auth_fields;
/** Group hashes and mode keys never name the empty string. */
constexpr std::uint32_t kEmptyName =
    middleware::content::packages::tables::activity_settings::kEmptyNameKey;
/** The highest native lifetime value a mode phase can name: the advance lifetime 8. */
constexpr std::uint8_t kLastPhase = match_state::kAdvance;
} // namespace

std::span<const Definition* const> supported() noexcept {
    static const std::array modes{&rumble::definition(), &control::definition()};
    return modes;
}

const Definition* find(std::uint32_t key) noexcept {
    for (const auto* mode : supported()) {
        if (mode->key == key) {
            return mode;
        }
    }
    return nullptr;
}

namespace {
/** A mode with capture zones scores teams and names distinct authored object slots. */
bool valid_zones(const Definition& mode) noexcept {
    const auto& zones = mode.zones;
    if (zones.count == 0) {
        return true;
    }
    if (mode.teamCount == 0 || zones.count > zones.placements.size()) {
        return false;
    }
    for (std::size_t i = 0; i < zones.count; ++i) {
        const auto& zone = zones.placements[i];
        if (zone.group == 0 || zone.group == kEmptyName || zone.slot >= auth::kMaximumClientRefIndex
            || zone.captureKey == 0) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto& other = zones.placements[j];
            if ((zone.group == other.group && zone.slot == other.slot)
                || zone.captureKey == other.captureKey) {
                return false;
            }
        }
    }
    return true;
}
} // namespace

bool valid(const Definition& mode) noexcept {
    if (!mode.scripted || mode.key == 0 || mode.key == kEmptyName || mode.name.empty()
        || mode.settingsTag == 0 || mode.scoreboardTag == 0 || mode.qualifyingKillKey == 0
        || mode.scoreNoticeKey == 0 || mode.load == nullptr || mode.validate == nullptr
        || mode.scriptedKillEligible == nullptr || mode.scriptedIntroBindings.empty()) {
        return false;
    }
    namespace policy = match_state::script_policy;
    std::uint32_t notices{};
    for (const auto& binding : mode.scriptedNoticeBindings) {
        if (binding.name <= policy::Notice::none || binding.name >= policy::Notice::count
            || binding.key == 0 || binding.key == kEmptyName) {
            return false;
        }
        const auto bit = 1U << static_cast<unsigned>(binding.name);
        if ((notices & bit) != 0) {
            return false;
        }
        notices |= bit;
    }
    if (mode.scriptedIntroBindings.size()
        > middleware::bap::activity_message::cinematic_record::kCommandCapacity) {
        return false;
    }
    for (std::size_t i = 0; i < mode.scriptedIntroBindings.size(); ++i) {
        const auto& binding = mode.scriptedIntroBindings[i];
        if (binding.name <= match_state::script_policy::IntroClip::none
            || binding.name >= match_state::script_policy::IntroClip::count
            || binding.clip.group == 0 || binding.clip.group == kEmptyName
            || binding.clip.slot >= auth::kMaximumClientRefIndex) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto& other = mode.scriptedIntroBindings[j];
            if (binding.name == other.name
                || (binding.clip.group == other.clip.group
                    && binding.clip.slot == other.clip.slot)) {
                return false;
            }
        }
    }
    const std::array phases{mode.phases.active,
                            mode.phases.completion,
                            mode.phases.postgame,
                            mode.phases.advance,
                            mode.phases.intro};
    for (std::size_t i = 0; i < phases.size(); ++i) {
        if (phases[i] > kLastPhase) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (phases[i] == phases[j]) {
                return false;
            }
        }
    }
    if (!valid_zones(mode) || mode.teamCount > auth::kTeamFactionCount) {
        return false;
    }
    return true;
}

} // namespace sunrise::state::activity::match_modes

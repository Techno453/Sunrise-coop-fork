#include <atomic>

#include "../../../../core/logging/log.h"
#include "../../../../middleware/content/packages/schema/generated/tag_classes_808077.h"
#include "../../../../middleware/content/packages/schema/generated/tag_classes_80807d.h"
#include "../../../../middleware/content/packages/tables/activity_definition_reader.h"
#include "../../../../middleware/content/packages/tables/activity_options.h"
#include "../../../../middleware/content/packages/tables/activity_rules.h"
#include "../../../../middleware/content/packages/tables/activity_settings.h"
#include "../../../../middleware/content/packages/tables/activity_table.h"
#include "../../../../state/build_data/activity_options/runtime.h"
#include "../../../../state/build_data/activity_rules/runtime.h"
#include "build.h"
#include "internal.h"

namespace sunrise::client::content::items::packages {
namespace {
namespace parsed = middleware::content::packages::tables::activity_options;
namespace domain = state::build_data::activity_options;
namespace generated = middleware::content::packages::schema::generated;
std::atomic_bool g_attempted{false};

/** Investment root slot of the activity type table, the mode families. */
constexpr std::size_t kModeTableSlot =
    tables::root_slot(offsetof(generated::InvestmentRoot, unk58));
/** Class of the installed activity type table. */
constexpr std::uint32_t kModeTableClass = generated::C80807773::kClass;

struct Context final {
    const reader::Source* source{};
    Storage* storage{};
};

bool read_option_tag(void* opaque,
                     std::uint32_t tag,
                     std::span<const std::byte>& bytes,
                     std::uint32_t& cls) noexcept {
    auto& context = *static_cast<Context*>(opaque);
    if (!reader::read_tag(
            *context.source, context.storage->scratch, tag, context.storage->definition, cls)) {
        return false;
    }
    bytes = context.storage->definition;
    return true;
}

bool build_match_rules(const reader::Source& source, Storage& storage) noexcept {
    namespace rules = state::build_data::activity_rules;
    std::uint32_t activityTag{}, modeTag{}, cls{};
    if (!tables::slot_tag(storage.root, tables::activities::kRootSlot, activityTag)
        || !tables::slot_tag(storage.root, kModeTableSlot, modeTag)) {
        return false;
    }
    std::vector<std::byte> activities, modes, settings, scoreboard, incidents;
    if (!reader::read_tag(source, storage.scratch, activityTag, activities, cls)
        || cls != tables::kActivityDefinitionTableClass
        || !reader::read_tag(source, storage.scratch, modeTag, modes, cls) || cls != kModeTableClass
        || !reader::read_tag(
            source, storage.scratch, tables::incident_targets::kCatalogTag, incidents, cls)
        || cls != tables::incident_targets::kCatalogClass) {
        return false;
    }
    tables::activity_rules::ActivityModes membership{};
    tables::incident_targets::Catalog targets{};
    if (!tables::activity_rules::parse_activity_modes(activities, modes, membership)
        || !tables::incident_targets::parse(incidents, targets)) {
        return false;
    }
    const auto definitions = state::activity::match_modes::supported();
    if (definitions.empty() || definitions.size() > rules::kModeCapacity) {
        return false;
    }
    std::vector<tables::activity_rules::ModeData> data(definitions.size());
    std::array<rules::LoadedMode, rules::kModeCapacity> loaded{};
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const auto* mode = definitions[i];
        if (mode == nullptr || !state::activity::match_modes::valid(*mode)
            || !reader::read_tag(source, storage.scratch, mode->settingsTag, settings, cls)
            || cls != tables::activity_settings::kTableClass
            || !reader::read_tag(source, storage.scratch, mode->scoreboardTag, scoreboard, cls)
            || cls != tables::statistic_program::kScoreboardClass
            || !mode->load(settings, scoreboard, data[i])) {
            return false;
        }
        loaded[i] = {mode, &data[i]};
    }
    return rules::publish(std::span(membership.values).first(membership.count),
                          std::span(loaded).first(definitions.size()),
                          targets);
}
/** True once the optional private-match option and mode rule pass has run. */
bool activity_options_finished() noexcept {
    return g_attempted.load(std::memory_order_acquire);
}
} // namespace

bool activity_options_due() noexcept {
    std::array<std::uint32_t, kContainerCandidates> candidates{};
    std::size_t count = 0;
    // Naming can precede content-catalog publication; wait for that prerequisite.
    return !activity_options_finished() && investment_globals_tags(candidates, count);
}

void build_activity_options(const reader::Source& source, Storage& storage) noexcept {
    if (activity_options_finished()) {
        return;
    }
    std::array<std::uint32_t, kContainerCandidates> candidates{};
    std::size_t count = 0;
    // The caller waits for activity_options_due(); every pass that starts here ends attempted,
    // so no content refresh waits on this optional domain.
    if (!investment_globals_tags(candidates, count)) {
        g_attempted.store(true, std::memory_order_release);
        core::log::write(core::log::Channel::state,
                         core::log::Level::warn,
                         "ev=activity_options stage=build result=unnamed");
        return;
    }
    static parsed::Catalog catalog{};
    static std::array<domain::Option, domain::kOptionCapacity> values{};
    bool published = false;
    std::uint32_t tableTag = 0;
    for (std::size_t i = 0; i < count && !published; ++i) {
        std::uint32_t rootTag = 0;
        std::uint32_t cls = 0;
        if (!reader::read_tag(source, storage.scratch, candidates[i], storage.container)
            || !tables::child_tag(storage.container, tables::kInvestmentRootChild, rootTag)
            || !reader::read_tag(source, storage.scratch, rootTag, storage.root, cls)
            || cls != tables::kInvestmentRootClass
            || !tables::slot_tag(storage.root, parsed::kRootSlot, tableTag)) {
            continue;
        }
        if (!reader::read_tag(source, storage.scratch, tableTag, storage.child, cls)
            || cls != parsed::kTableClass) {
            continue;
        }
        Context context{&source, &storage};
        if (!parsed::parse(storage.child, &context, &read_option_tag, catalog)) {
            continue;
        }
        values = {};
        for (std::size_t n = 0; n < catalog.count; ++n) {
            const auto& from = catalog.options[n];
            values[n] = {from.settingKey, from.variantCount, from.presentMask, from.values};
        }
        published = domain::publish(std::span(values).first(catalog.count));
    }
    const bool rulesPublished = published && build_match_rules(source, storage);
    core::log::write(core::log::Channel::state,
                     rulesPublished ? core::log::Level::info : core::log::Level::warn,
                     rulesPublished ? "ev=activity_rules stage=build result=ok modes=registered"
                                    : "ev=activity_rules stage=build result=unavailable");
    // Unsupported content disables matches without blocking sign-in or retrying malformed data.
    g_attempted.store(true, std::memory_order_release);
    core::log::writef(core::log::Channel::state,
                      published ? core::log::Level::info : core::log::Level::warn,
                      "ev=activity_options stage=build result=%s tag=0x%08X count=%u",
                      published ? "ok" : "invalid",
                      tableTag,
                      published ? catalog.count : 0U);
}
} // namespace sunrise::client::content::items::packages

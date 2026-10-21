#include "starter_set_catalog.h"

#include <mutex>
#include <shared_mutex>

#include "core/threading/srw_lock.h"

namespace sunrise::state::build_data::starter_sets {
namespace {

core::threading::SrwLock g_lock;
/** Indexed by class byte. */
std::array<Definition, kClassCount> g_rows{};
std::array<bool, kClassCount> g_present{};
bool g_published{};

} // namespace

/** Clears the published rows under the catalog lock. */
void clear() noexcept {
    const std::lock_guard guard(g_lock);
    g_rows = {};
    g_present = {};
    g_published = false;
}

/**
 * Publishes the whole table in one step. An empty table is published too.
 * @param definitions One row per class, each class at most once.
 * @return False when a row names no playable class, repeats one, or overflows its items.
 */
bool replace(std::span<const Definition> definitions) noexcept {
    std::array<Definition, kClassCount> rows{};
    std::array<bool, kClassCount> present{};
    for (const Definition& definition : definitions) {
        if (definition.characterClass >= kClassCount || present[definition.characterClass]
            || definition.itemCount > kItemCapacity) {
            return false;
        }
        rows[definition.characterClass] = definition;
        present[definition.characterClass] = true;
    }
    const std::lock_guard guard(g_lock);
    g_rows = rows;
    g_present = present;
    g_published = true;
    return true;
}

/** @return True once a table, empty or not, is published. */
bool ready() noexcept {
    const std::shared_lock guard(g_lock);
    return g_published;
}

/** Finds the row of one class under the catalog lock. */
bool find(std::uint8_t characterClass, Definition& definition) noexcept {
    definition = {};
    const std::shared_lock guard(g_lock);
    if (characterClass >= kClassCount || !g_present[characterClass]) {
        return false;
    }
    definition = g_rows[characterClass];
    return true;
}

} // namespace sunrise::state::build_data::starter_sets

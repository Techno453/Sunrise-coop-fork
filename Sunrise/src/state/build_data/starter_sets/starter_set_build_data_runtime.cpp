#include "../runtime.h"
#include "starter_set_catalog.h"

namespace sunrise::state::build_data {

/** @return True once the new-character table, empty or not, is in State. */
bool starter_sets_ready() noexcept {
    return starter_sets::ready();
}

/** Publishes the new-character table outside the disk cache, so a warm boot reads it again. */
bool publish_starter_sets(std::span<const starter_sets::Definition> definitions) noexcept {
    return starter_sets::replace(definitions);
}

/** Finds the items the packages give a new character of one class. */
bool find_starter_set(std::uint8_t characterClass, starter_sets::Definition& definition) noexcept {
    return starter_sets::find(characterClass, definition);
}

} // namespace sunrise::state::build_data

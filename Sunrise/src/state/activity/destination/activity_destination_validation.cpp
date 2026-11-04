#include "activity_destination_validation.h"

namespace sunrise::state::activity::destination {

/** Bounds one destination selection against its fixed package-name storage. */
bool valid(const DestinationSelection& selection) noexcept {
    if (selection.packageNameLength > selection.packageName.size()
        || selection.optionPairCount > selection.optionPairs.size()
        || (!selection.hasOptionPairs && selection.optionPairCount != 0)) {
        return false;
    }
    for (std::size_t i = 0; i < selection.optionPairs.size(); ++i) {
        if (selection.optionPairs[i] > build_data::activity_options::kMaximumPair
            || (i >= selection.optionPairCount && selection.optionPairs[i] != 0)) {
            return false;
        }
    }
    return true;
}

} // namespace sunrise::state::activity::destination

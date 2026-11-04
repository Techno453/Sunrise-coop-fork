#include <Windows.h>

#include <span>

#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "../../build_data/activity_options/runtime.h"
#include "../../runtime/storage/internal.h"
#include "../activity_clock/definition.h"
#include "../destination/activity_destination_validation.h"
#include "../transactions/internal.h"
#include "from_selection.h"

namespace sunrise::state::activity::activity_timer {

bool resolve_selected(std::uint64_t sessionId,
                      std::string_view packageName,
                      ResolvedTimer& output) noexcept {
    namespace options = build_data::activity_options;
    if (sessionId == kAbsentSessionId
        || !packageName.starts_with(activity_clock::kPvpPackagePrefix)) {
        return false;
    }
    destination::DestinationSelection selection{};
    std::uint64_t createdRevision = 0;
    bool found = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& state = runtime::storage::g_state.activity;
    const std::size_t slot = transactions::find_session(state, sessionId);
    if (slot != kInvalidSessionSlot) {
        selection = state.sessions[slot].destination;
        createdRevision = state.sessions[slot].createdRevision;
        found = true;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!found || !destination::valid(selection) || !selection.hasOptionPairs
        || std::string_view(reinterpret_cast<const char*>(selection.packageName.data()),
                            selection.packageNameLength)
               != packageName) {
        return false;
    }
    std::int32_t seconds = 0;
    if (!options::resolve(std::span(selection.optionPairs).first(selection.optionPairCount),
                          middleware::content::packages::tables::activity_settings::kTimeLimitKey,
                          seconds)
        || seconds <= 0) {
        return false;
    }
    ResolvedTimer result{};
    result.maximum = static_cast<std::uint64_t>(seconds) * activity_clock::kTicksPerSecond;
    result.remaining = result.maximum;
    result.rate = 1.0F;

    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& current = runtime::storage::g_state.activity;
    const std::size_t currentSlot = transactions::find_session(current, sessionId);
    // A released/reallocated session must not receive the prior selection's timer.
    if (currentSlot != kInvalidSessionSlot) {
        auto& record = current.sessions[currentSlot];
        const auto& now = record.destination;
        found = record.createdRevision == createdRevision
                && now.packageName == selection.packageName
                && now.packageNameLength == selection.packageNameLength
                && now.activityIndex == selection.activityIndex
                && now.hasOptionPairs == selection.hasOptionPairs
                && now.optionPairCount == selection.optionPairCount
                && now.optionPairs == selection.optionPairs;

    } else {
        found = false;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (!found) {
        return false;
    }
    output = result;
    return true;
}
} // namespace sunrise::state::activity::activity_timer

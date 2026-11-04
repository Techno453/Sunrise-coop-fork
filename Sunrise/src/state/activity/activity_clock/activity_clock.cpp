#include <Windows.h>

#include "../../runtime/storage/internal.h"
#include "../transactions/internal.h"
#include "definition.h"
#include "runtime.h"

namespace sunrise::state::activity::activity_clock {
bool resolve(std::uint64_t sessionId,
             std::string_view packageName,
             std::uint64_t nowMilliseconds,
             std::uint64_t& ticks) noexcept {
    if (sessionId == kAbsentSessionId || !packageName.starts_with(kPvpPackagePrefix)) {
        return false;
    }
    bool found = false;
    std::uint64_t result = 0;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t target = transactions::find_session(state, sessionId);
    if (target != kInvalidSessionSlot) {
        SessionRecord& record = state.sessions[target];
        const auto& selection = record.destination;
        if (selection.packageNameLength <= selection.packageName.size()
            && std::string_view(reinterpret_cast<const char*>(selection.packageName.data()),
                                selection.packageNameLength)
                   == packageName) {
            result = advance(record.activityClock, nowMilliseconds);
            found = true;
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (!found) {
        return false;
    }
    ticks = result;
    return true;
}
} // namespace sunrise::state::activity::activity_clock

#pragma once
#include "../../build_data/activity_rules/runtime.h"
#include "../member_selection.h"
#include "runtime.h"
namespace sunrise::state::activity::match_state {
inline const membership::MembershipState*
current_member(const SessionRecord& record, std::uint64_t key, bool player = false) noexcept {
    if (key == 0 || key == UINT64_MAX) {
        return nullptr;
    }
    for (std::size_t row = 0; row < kInvalidMemberRow; ++row) {
        const auto* state = member_state(record, row);
        const auto* identity = member_identity(record, row);
        if (state && identity && state->hasIdentity
            && (player ? state->identity.opaqueSoid : identity->memberKey) == key) {
            return state;
        }
    }
    return nullptr;
}

/** Resolves the supported rules of one allocation from its requested activity. */
inline bool resolve_rules(const SessionRecord& record,
                          build_data::activity_rules::Rules& rules) noexcept {
    return build_data::activity_rules::resolve(match_activity(record.destination), rules);
}
} // namespace sunrise::state::activity::match_state

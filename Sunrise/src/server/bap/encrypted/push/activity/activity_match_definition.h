#pragma once

#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../../state/activity/match_state/script_policy.h"

namespace sunrise::server::bap::encrypted::push::activity {

/** Applies the selected native adapters, never native spawn coordinates or relationship logic. */
[[nodiscard]] inline bool apply_match_definition(
    const state::activity::match_state::script_policy::ModeDefinition& mode,
    middleware::bap::activity_message::sensor_auth_update::Snapshot& snapshot) noexcept {
    namespace policy = state::activity::match_state::script_policy;
    if (!policy::supported(mode)
        || snapshot.participationSeatCount > snapshot.participationSeats.size()) {
        return false;
    }
    // Both adapters publish committed native partitions; the client evaluates relationships.
    snapshot.partitionFactions = true;
    snapshot.matchTeamCount = mode.teamCount;
    snapshot.matchTeamScoring = mode.teamScoring;
    snapshot.inheritRespawnTimers = mode.respawn == policy::RespawnPolicy::installed;
    // rounds=single is checked against installed content before this projection is accepted.
    return true;
}

} // namespace sunrise::server::bap::encrypted::push::activity

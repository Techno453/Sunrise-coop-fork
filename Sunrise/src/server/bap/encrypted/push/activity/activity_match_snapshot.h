#pragma once
#include "internal.h"
namespace sunrise::server::bap::encrypted::push::activity {
/**
 * Adds the match a PvP activity is playing to one roster snapshot.
 * @param session Recipient connection.
 * @param snapshot Roster snapshot to extend.
 * @param name Destination package name; only a PvP package carries a match.
 * @return True when the snapshot carries the match or the activity has none.
 */
[[nodiscard]] bool
fill_match_snapshot(Session& session, message::Snapshot& snapshot, std::string_view name) noexcept;
} // namespace sunrise::server::bap::encrypted::push::activity

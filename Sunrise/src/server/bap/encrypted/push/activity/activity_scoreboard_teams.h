#pragma once

#include "../../../../../middleware/bap/activity_message/auth_fields.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../../state/build_data/activity_rules/runtime.h"

namespace sunrise::server::bap::encrypted::push::activity {

/** Score column 0 is verified in the installed mode config before rules are published. */
inline void
fill_match_scores(const middleware::bap::activity_message::sensor_auth_update::Snapshot& snapshot,
                  middleware::bap::activity_message::scoreboard_record::Record& record) noexcept {
    if (!snapshot.hasMatchScores || record.elementCount > record.elements.size()
        || record.teamCount > record.teams.size() || snapshot.matchPlayerStatisticCount == 0
        || snapshot.matchPlayerStatisticCount > record.elements[0].statistics.values.size()
        || snapshot.matchTeamStatisticCount == 0
        || snapshot.matchTeamStatisticCount > record.teams[0].statistics.values.size()) {
        return;
    }
    for (std::size_t i = 0; i < record.elementCount; ++i) {
        auto& element = record.elements[i];
        element.statistics.present = true;
        element.statistics.count = static_cast<std::uint8_t>(snapshot.matchPlayerStatisticCount);
        element.statistics.values = {};
        for (const auto& value : snapshot.matchStatistics) {
            if (value.playerKey == element.key) {
                element.statistics.values = value.values;
                break;
            }
        }
        for (const auto& value : snapshot.matchScores) {
            if (value.playerKey == element.key) {
                element.statistics.values[0] = value.value;
                break;
            }
        }
    }
    namespace fields = middleware::bap::activity_message::auth_fields;
    // Team rows are named by faction; scores and seats are kept by partition.
    for (std::size_t t = 0; t < record.teamCount; ++t) {
        auto& team = record.teams[t];
        const auto partition = fields::faction_team(team.faction);
        team.statistics.present = true;
        team.statistics.count = static_cast<std::uint8_t>(snapshot.matchTeamStatisticCount);
        team.statistics.values = {};
        // A team mode publishes its retained total, which keeps points of departed members.
        if (snapshot.matchTeamScoring) {
            for (const auto& total : snapshot.matchTeamScores) {
                if (total.faction >= 0 && total.faction == partition) {
                    team.statistics.values[0] = total.value;
                }
            }
            continue;
        }
        for (std::size_t s = 0; s < snapshot.participationSeatCount; ++s) {
            const auto& seat = snapshot.participationSeats[s];
            if (!seat.hasAssignedPartition || seat.assignedPartition != partition) {
                continue;
            }
            for (std::size_t e = 0; e < record.elementCount; ++e) {
                const auto player =
                    seat.scoreboardPlayerKey ? seat.scoreboardPlayerKey : seat.playerKey;
                if (record.elements[e].key == player) {
                    team.statistics.values[0] += record.elements[e].statistics.values[0];
                }
            }
        }
    }
}

/** Project mode teams and complete native assignments from the same roster snapshot. */
[[nodiscard]] inline bool fill_scoreboard_teams(
    const middleware::bap::activity_message::sensor_auth_update::Snapshot& snapshot,
    middleware::bap::activity_message::scoreboard_record::Record& record) noexcept {
    namespace message = middleware::bap::activity_message;
    namespace fields = message::auth_fields;
    if (!snapshot.partitionFactions || record.elementCount == 0
        || record.elementCount > record.elements.size()
        || snapshot.matchTeamCount > record.teams.size()
        || snapshot.participationSeatCount > snapshot.participationSeats.size()) {
        return false;
    }
    std::array<message::scoreboard_record::Team, message::scoreboard_record::kTeamCount> teams{};
    std::size_t count = snapshot.matchTeamCount;
    for (std::size_t index = 0; index < count; ++index) {
        teams[index] = {fields::team_faction(static_cast<std::int8_t>(index)), true, true};
    }
    for (std::size_t index = 0; index < record.elementCount; ++index) {
        const auto& element = record.elements[index];
        if (!element.live || !element.hasKey || element.key == 0) {
            return false;
        }
        std::int8_t partition = message::sensor_auth_update::kUnassignedPartition;
        if (snapshot.perMemberParticipation) {
            for (std::size_t seatIndex = 0; seatIndex < snapshot.participationSeatCount;
                 ++seatIndex) {
                const auto& seat = snapshot.participationSeats[seatIndex];
                const auto player =
                    seat.scoreboardPlayerKey ? seat.scoreboardPlayerKey : seat.playerKey;
                if (player == element.key && seat.hasAssignedPartition) {
                    if (partition != message::sensor_auth_update::kUnassignedPartition
                        && partition != seat.assignedPartition) {
                        return false;
                    }
                    partition = seat.assignedPartition;
                }
            }
        }
        // Incomplete assignments make no team, although a player_spawned report (incident type
        // 13) can publish faction 0 before native membership arrives.
        if (partition < 0 || partition > message::sensor_auth_update::kLastPartition
            || (snapshot.matchTeamCount != 0 && partition >= snapshot.matchTeamCount)) {
            return false;
        }
        // Teams stay in partition order.
        std::size_t position = 0;
        while (position < count && fields::faction_team(teams[position].faction) < partition) {
            ++position;
        }
        if (position < count && fields::faction_team(teams[position].faction) == partition) {
            continue;
        }
        if (count == teams.size()) {
            return false;
        }
        for (std::size_t move = count; move > position; --move) {
            teams[move] = teams[move - 1];
        }
        // The team row carries the faction its players carry, from which native 0xFDA040 takes
        // the partition the Switch Teams chooser cycles over.
        teams[position] = {fields::team_faction(partition), true, true};
        ++count;
    }
    record.teams = teams;
    record.teamCount = static_cast<std::uint8_t>(count);
    record.hasTeamList = true;
    return true;
}

} // namespace sunrise::server::bap::encrypted::push::activity

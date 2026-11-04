#include "activity_match_snapshot.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <memory>
#include <new>

#include "../../../../../middleware/bap/activity_message/capture_zone_auth.h"
#include "../../../../../state/activity/activity_clock/runtime.h"
#include "../../../../../state/activity/activity_timer/from_selection.h"
#include "../../../../../state/activity/match_state/runtime.h"
#include "../../../../activity/host_runtime.h"
#include "activity_match_definition.h"
#include "activity_match_publication.h"
#include "activity_scoreboard_teams.h"
namespace sunrise::server::bap::encrypted::push::activity {
namespace {
namespace match = state::activity::match_state;
/**
 * Generation of capture objects, which remain spawned for the match.
 */
constexpr std::int32_t kZoneGeneration = 1;

/**
 * Spawns the mode's capture zones as match objects. Each capture block carries the authority's
 * owner and its owner revision. The peer simulating a zone sets the owner itself when its capture
 * completes (0xED5070 -> 0xED7C20) and takes the block's owner only from a newer revision
 * (0xED7820 -> 0xED7AD0), so the block re-asserts an accepted owner and reverts a refused one; a
 * zone nobody captured yet stays neutral at revision zero.
 */
void fill_match_zones(const state::activity::match_modes::Definition& mode,
                      const state::activity::match_state::State& result,
                      message::Snapshot& snapshot) noexcept {
    namespace capture_zone = middleware::bap::activity_message::capture_zone;
    static_assert(state::activity::match_modes::kZonePlacementCapacity
                  <= middleware::bap::activity_message::match_object::kCapacity);
    const auto& zones = mode.zones;

    for (std::size_t i = 0;
         i < zones.count && snapshot.matchObjectCount < snapshot.matchObjects.size();
         ++i) {
        capture_zone::State state{};
        // The block names the zone by its authored position, the HUD record's first field.
        state.index = static_cast<std::int32_t>(i);
        for (const auto& hold : result.zones) {
            if (hold.placement >= 0 && static_cast<std::size_t>(hold.placement) == i) {
                state.ownerRevision = hold.ownerRevision;
                // The authority keeps a zone's owning partition; the block names its faction.
                state.owner =
                    hold.owner >= 0
                        ? middleware::bap::activity_message::auth_fields::team_faction(hold.owner)
                        : capture_zone::kNeutral;
            }
        }
        if (capture_zone::object(snapshot.matchObjects[snapshot.matchObjectCount],
                                 zones.placements[i].group,
                                 zones.placements[i].slot,
                                 kZoneGeneration,
                                 state,
                                 lists_hud_zones(mode))) {
            ++snapshot.matchObjectCount;
        }
    }
}

/**
 * Lists the mode's capture zones in the hud_sensor, in authored (letter) order, for the objective
 * HUD that letters them; every other mode keeps the list empty.
 */
void fill_hud_zones(const state::activity::match_modes::Definition& mode,
                    message::Snapshot& snapshot) noexcept {
    static_assert(state::activity::match_modes::kZonePlacementCapacity
                  <= message::kHudZoneCapacity);
    const auto& zones = mode.zones;
    snapshot.hudZoneCount = 0;
    if (!lists_hud_zones(mode)) {
        return;
    }
    for (std::size_t i = 0; i < zones.count; ++i) {
        snapshot.hudZones[i] = {zones.placements[i].group, zones.placements[i].slot};
    }
    snapshot.hudZoneCount = zones.count;
}

bool fill_aborted_match(const Session& session,
                        message::Snapshot& snapshot,
                        const message::Snapshot& base,
                        const match::State& state) noexcept {
    snapshot = base;
    snapshot.matchAborted = true;
    snapshot.matchFinished = snapshot.matchComplete = snapshot.matchIntro = false;
    snapshot.hasMatchScores = false;
    snapshot.matchAllocationRevision = state.allocationRevision;
    snapshot.matchRevision = state.revision;
    snapshot.matchCinematics = {};
    snapshot.matchObjectCount = snapshot.hudZoneCount = 0;
    snapshot.matchObjectiveHud = {};
    snapshot.activityTimerActive = snapshot.matchClockVisible = false;
    if (snapshot.lifetime != server::activity::host::kLoadingLifetimeState) {
        snapshot.lifetime = match_abort_phase(session, state.allocationRevision);
    }
    for (auto& row : snapshot.scoreboard.elements) {
        row.hudTitle = -1;
    }
    for (auto& row : snapshot.scoreboard.teams) {
        row.hudTitle = -1;
    }
    snapshot.scoreboard.globalStatistics = {};
    return true;
}
} // namespace

bool fill_match_snapshot(Session& session,
                         message::Snapshot& snapshot,
                         std::string_view name) noexcept {
    namespace match = state::activity::match_state;
    const auto id = session.activity.session.sessionId;
    state::build_data::activity_rules::Rules rules{};
    match::State result{};
    if (!name.starts_with(state::activity::activity_clock::kPvpPackagePrefix)
        || !match::snapshot(id, result, &rules)) {
        return true;
    }
    if (result.aborted) {
        return fill_aborted_match(session, snapshot, snapshot, result);
    }
    if (!result.scriptFlowReady) {
        return false;
    }
    const std::unique_ptr<message::Snapshot> base(new (std::nothrow) message::Snapshot(snapshot));
    if (!base) {
        return false;
    }
    snapshot.matchClock =
        state::activity::activity_clock::resolve(id, name, GetTickCount64(), snapshot.activityTime);
    bool introAvailable = true;
    const auto introBindings = rules.mode->scriptedIntroBindings;
    for (const auto& binding : introBindings) {
        const auto& clip = binding.clip;
        bool found = false;
        for (std::size_t g = 0; g < snapshot.roster.groupCount; ++g) {
            const auto& group = snapshot.roster.groups[g];
            for (std::size_t s = 0; s < group.slotTypes.size(); ++s) {
                found =
                    found
                    || (group.key == clip.group && group.slotIndices[s] == clip.slot
                        && group.slotTypes[s]
                               == middleware::bap::activity_message::cinematic_record::kSlotType);
            }
        }
        introAvailable = introAvailable && found;
    }
    // Binding a seat makes no member ready, so it leaves the intro and the snapshot unchanged.
    if (introAvailable && snapshot.participationSeatCount != 0) {
        match::bind_intro_member(id,
                                 session.activityMemberKey,
                                 snapshot.roster.playerKeyGroup,
                                 snapshot.participationSeats[0].slotIndex);
    }
    match::script_policy::ModeDefinition definition{};
    if (snapshot.participationSeatCount > snapshot.participationSeats.size()) {
        return false;
    }
    if (!match::script_definition(id, result.allocationRevision, definition)) {
        if (match::snapshot(id, result) && result.aborted) {
            return fill_aborted_match(session, snapshot, *base, result);
        }
        return false;
    }

    snapshot.matchModeKey = rules.mode->key;
    auto objects = *rules.mode;
    {
        if (definition.objectives == match::script_policy::Objective::none) {
            objects.zones = {};
        }
        if (definition.hud == match::script_policy::Hud::none) {
            objects.objectiveHud = {};
        }
    }
    snapshot.matchObjectiveHud = objects.objectiveHud;
    snapshot.matchIntro = match::intro_active(result);
    snapshot.matchFinished = result.endReason != match::EndReason::none;
    match::Publication publication{result.allocationRevision,
                                   result.revision,
                                   snapshot.matchFinished,
                                   result.advanceReady,
                                   result.postgameReady,
                                   rules.mode,
                                   snapshot.matchIntro};
    publication.scriptFlow = result.scriptFlow;
    publication.scriptFlowReady = result.scriptFlowReady;
    const std::uint8_t phase = match_publication_phase(session, publication);
    // The base roster shows the loading presentation and refuses the native spawn gate until
    // this client reports its region ready; the match phase replaces only the active state it
    // would otherwise send.
    if (snapshot.lifetime != server::activity::host::kLoadingLifetimeState) {
        snapshot.lifetime = phase;
    }
    snapshot.matchComplete = snapshot.matchFinished && phase != rules.mode->phases.active;
    snapshot.matchCinematics = result.cinematics;
    snapshot.matchObjectCount = 0;
    fill_match_zones(objects, result, snapshot);
    fill_hud_zones(objects, snapshot);
    snapshot.hasMatchScores = true;
    snapshot.matchRevision = result.revision;
    snapshot.matchTimedRevisions = result.timedRevisions;
    snapshot.matchStatisticFrames = result.statisticFrames;
    snapshot.matchAllocationRevision = result.allocationRevision;
    snapshot.matchPlayerStatisticCount = rules.settings.playerStatisticCount;
    snapshot.matchTeamStatisticCount = rules.settings.teamStatisticCount;
    for (std::size_t i = 0; i < result.scores.size(); ++i) {
        snapshot.matchScores[i] = {result.scores[i].playerKey,
                                   static_cast<float>(result.scores[i].value)};
    }
    for (std::size_t i = 0; i < result.statistics.size(); ++i) {
        snapshot.matchStatistics[i] = {result.statistics[i].playerKey, result.statistics[i].values};
    }
    static_assert(std::tuple_size_v<decltype(result.teamScores)>
                  == std::tuple_size_v<decltype(snapshot.matchTeamScores)>);
    snapshot.matchTeamScoring = definition.teamScoring;
    for (std::size_t i = 0; snapshot.matchTeamScoring && i < result.teamScores.size(); ++i) {
        snapshot.matchTeamScores[i] = {result.teamScores[i].partition,
                                       static_cast<float>(result.teamScores[i].value)};
    }
    std::array<std::uint64_t, match::kPlayerCapacity> players{};
    std::array<match::MemberProjection, match::kPlayerCapacity> participation{};
    if (snapshot.participationSeatCount > players.size()) {
        return false;
    }
    for (std::size_t i = 0; i < snapshot.participationSeatCount; ++i) {
        players[i] = i == 0 && snapshot.hasScoreboard ? snapshot.scoreboard.elements[0].key
                                                      : snapshot.participationSeats[i].playerKey;
    }
    if (!match::project_members(id,
                                result.allocationRevision,
                                std::span(players).first(snapshot.participationSeatCount),
                                std::span(participation).first(snapshot.participationSeatCount))) {
        if (match::snapshot(id, result) && result.aborted) {
            return fill_aborted_match(session, snapshot, *base, result);
        }
        return false;
    }
    for (std::size_t i = 0; i < snapshot.participationSeatCount; ++i) {
        auto& seat = snapshot.participationSeats[i];
        seat.scoreboardPlayerKey = players[i];
        seat.hasAssignedPartition = participation[i].assigned;
        seat.assignedPartition = participation[i].partition;
    }
    if (!apply_match_definition(definition, snapshot)) {
        return false;
    }
    if (snapshot.hasScoreboard) {
        snapshot.scoreboard.hasLayout = true;
        snapshot.scoreboard.layoutHash =
            middleware::bap::activity_message::scoreboard_record::kMatchLayout;
        static_cast<void>(fill_scoreboard_teams(snapshot, snapshot.scoreboard));
        fill_match_scores(snapshot, snapshot.scoreboard);
        snapshot.scoreboard.globalStatistics = {};
        for (std::size_t i = 0; i < snapshot.scoreboard.teamCount; ++i) {
            auto& row = snapshot.scoreboard.teams[i];
            row.hudTitle = -1;
        }
        if (!match::project_script_results(id, result.allocationRevision, snapshot.scoreboard)) {
            if (match::snapshot(id, result) && result.aborted) {
                return fill_aborted_match(session, snapshot, *base, result);
            }
            return false;
        }
    }
    state::activity::activity_timer::ResolvedTimer timer{};
    bool timed = state::activity::activity_timer::resolve_selected(id, name, timer);
    // Use the installed time when the private lobby has not selected an override.
    const auto installedSeconds = rules.settings.timeLimitSeconds;
    if (!timed && installedSeconds > 0) {
        timer.maximum = static_cast<std::uint64_t>(installedSeconds)
                        * state::activity::activity_clock::kTicksPerSecond;
        timer.remaining = timer.maximum;
        timer.rate = 1.0F;
        timed = true;
    }
    if (timed) {
        match::match_timer(result, timer);
        if (snapshot.matchFinished) {
            match::completion_timer(result,
                                    phase,
                                    result.scriptFlow.completeSeconds,
                                    result.scriptFlow.postgameSeconds,
                                    timer);
        }
        snapshot.activityTimerActive = true;
        snapshot.activityTimerMaximum = timer.maximum;
        snapshot.activityTimerRemaining = timer.remaining;
        snapshot.activityTimerCurrentAtEpoch = timer.currentAtEpoch;
        snapshot.activityTimerEpoch = timer.epoch;
        snapshot.activityTimerRate = timer.rate;
        snapshot.activityTimerTailValue = timer.tailValue;
    }
    snapshot.matchClockVisible = match_clock_visible(timed, snapshot.lifetime, *rules.mode);
    return true;
}
} // namespace sunrise::server::bap::encrypted::push::activity

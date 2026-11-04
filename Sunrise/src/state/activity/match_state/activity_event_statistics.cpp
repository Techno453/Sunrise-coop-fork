#include <Windows.h>

#include <array>
#include <bit>
#include <cmath>

#include "../../../core/logging/log.h"
#include "../../../middleware/bap/activity_message/incident.h"
#include "../../../middleware/bap/activity_message/incident_player_roles.h"
#include "../../runtime/storage/internal.h"
#include "../transactions/internal.h"
#include "member.h"
#include "runtime.h"

namespace sunrise::state::activity::match_state {
namespace {
namespace incident = middleware::bap::activity_message::incident;
namespace programs = middleware::content::packages::tables::statistic_program;
namespace targets = middleware::content::packages::tables::incident_targets;
static_assert(script_policy::kClassificationCapacity == targets::kSelectionCapacity);

/** The completed event list of one report names `key`. */

const membership::MembershipState*
find_member(const SessionRecord& record, std::uint64_t key, bool player) noexcept {
    return current_member(record, key, player);
}

std::size_t column(const programs::Catalog& catalog, std::uint32_t key) noexcept {
    for (std::size_t i = 0; i < catalog.playerColumnCount; ++i) {
        if (catalog.playerColumns[i] == key) {
            return i;
        }
    }
    return catalog.playerColumns.size();
}

/** Installed expression lists also contain faction programs; this evaluator owns player state. */
bool team_only(const programs::Catalog& catalog, const programs::Program& program) noexcept {
    if (column(catalog, program.outputKey) < catalog.playerColumns.size()) {
        return false;
    }
    for (std::size_t i = 0; i < catalog.teamColumnCount && i < catalog.teamColumns.size(); ++i) {
        if (catalog.teamColumns[i] == program.outputKey) {
            return true;
        }
    }
    // An unknown output still fails the existing player-column validation.
    return false;
}

bool evaluate(const programs::Catalog& catalog,
              const programs::Program& program,
              PlayerStatistics& player,
              bool& changed) noexcept {
    std::array<programs::Vector, programs::kVariableCapacity> inputs{};
    if (program.variableCount > inputs.size()) {
        return false;
    }
    const auto output = column(catalog, program.outputKey);
    if (output >= player.values.size()) {
        return false;
    }
    for (std::size_t i = 0; i < program.variableCount; ++i) {
        const auto index = column(catalog, program.variables[i]);
        if (index >= player.values.size()) {
            return false;
        }
        // Native scoreboard statistics are scalar floats. The statistic program instruction set
        // has no cross-lane operations, so broadcasting preserves the exact first lane.
        inputs[i].fill(player.values[index]);
    }
    programs::Vector result{};
    if (!programs::evaluate(program, std::span(inputs).first(program.variableCount), result)) {
        return false;
    }
    changed = changed
              || std::bit_cast<std::uint32_t>(player.values[output])
                     != std::bit_cast<std::uint32_t>(result[0]);
    player.values[output] = result[0];
    return true;
}

/**
 * Frames one call advances the time recurrences. A pump refreshes the match every publish
 * interval, so live play never falls this far behind; after a pause with no link pumping, later
 * refreshes catch up this many frames at a time, which bounds the work done under State's lock.
 */
constexpr std::uint64_t kCatchUpFrames = activity_clock::kSimulationFramesPerSecond * 60;

bool time_columns(const programs::Catalog& catalog,
                  std::array<bool, programs::kPlayerStatisticCapacity>& timed,
                  std::array<bool, programs::kPlayerStatisticCapacity>& inputs) noexcept {
    if (catalog.playerColumnCount == 0 || catalog.playerColumnCount > catalog.playerColumns.size()
        || catalog.eventCount > catalog.events.size()
        || catalog.computedCount > catalog.computed.size()) {
        return false;
    }
    for (std::size_t i = 0; i < catalog.playerColumnCount; ++i) {
        timed[i] = (catalog.playerColumnMetadata[i] & programs::kColumnFormatMask)
                   == programs::kTimeColumnFormat;
    }
    for (std::size_t pass = 0; pass < catalog.computedCount; ++pass) {
        for (std::size_t i = 0; i < catalog.computedCount; ++i) {
            const auto& program = catalog.computed[i];
            if (team_only(catalog, program)) {
                continue;
            }
            const auto output = column(catalog, program.outputKey);
            if (output >= timed.size() || program.variableCount > program.variables.size()) {
                return false;
            }
            for (std::size_t v = 0; v < program.variableCount; ++v) {
                const auto input = column(catalog, program.variables[v]);
                if (input >= timed.size()) {
                    return false;
                }
                timed[output] = timed[output] || timed[input];
            }
        }
    }
    inputs = timed;
    for (std::size_t pass = 0; pass < catalog.computedCount; ++pass) {
        for (std::size_t i = 0; i < catalog.computedCount; ++i) {
            const auto& program = catalog.computed[i];
            if (team_only(catalog, program)) {
                continue;
            }
            if (!inputs[column(catalog, program.outputKey)]) {
                continue;
            }
            for (std::size_t v = 0; v < program.variableCount; ++v) {
                inputs[column(catalog, program.variables[v])] = true;
            }
        }
    }
    return true;
}

bool apply(const SessionRecord& record,
           const programs::Catalog& catalog,
           const targets::Catalog& bank,
           const targets::Selection& selection,
           const incident::PlayerRoles& roles,
           std::array<PlayerStatistics, kPlayerCapacity>& players,
           bool& changed) noexcept {
    std::array<bool, programs::kPlayerStatisticCapacity> timed{}, timeInputs{};
    if (!time_columns(catalog, timed, timeInputs)) {
        return false;
    }
    for (std::size_t e = 0; e < selection.count; ++e) {
        std::array<bool, kPlayerCapacity> touched{};
        const auto eventKey = bank.targets[selection.targets[e]].key;
        for (std::size_t p = 0; p < catalog.eventCount; ++p) {
            const auto& program = catalog.events[p];
            if (program.eventKey != eventKey) {
                continue;
            }
            if (team_only(catalog, program)) {
                continue;
            }
            const auto output = column(catalog, program.outputKey);
            if (output >= timed.size()) {
                return false;
            }
            // Loading can supply native spawn/death rate facts, but awards start with play.
            if (!record.matchState.hasStarted && !timeInputs[output]) {
                continue;
            }
            for (std::size_t r = 0; r < roles.count; ++r) {
                const auto& role = roles.roles[r];
                if (role.key != program.roleKey) {
                    continue;
                }
                for (std::size_t k = 0; k < role.count; ++k) {
                    const auto key = role.players[k];
                    if (find_member(record, key, true) == nullptr) {
                        continue;
                    }
                    std::size_t slot = players.size();
                    for (std::size_t i = 0; i < players.size(); ++i) {
                        if (players[i].playerKey == key) {
                            slot = i;
                            break;
                        }
                        if (slot == players.size() && players[i].playerKey == 0) {
                            slot = i;
                        }
                    }
                    if (slot == players.size()) {
                        return false;
                    }
                    changed = changed || players[slot].playerKey == 0;
                    players[slot].playerKey = key;
                    if (!evaluate(catalog, program, players[slot], changed)) {
                        return false;
                    }
                    touched[slot] = true;
                }
            }
        }
        for (std::size_t i = 0; i < players.size(); ++i) {
            if (!touched[i]) {
                continue;
            }
            for (std::size_t p = 0; p < catalog.computedCount; ++p) {
                const auto& program = catalog.computed[p];
                if (team_only(catalog, program)) {
                    continue;
                }
                if (record.matchState.hasStarted && !timed[column(catalog, program.outputKey)]
                    && !evaluate(catalog, program, players[i], changed)) {
                    return false;
                }
            }
        }
    }
    return true;
}
} // namespace

void advance_statistics(SessionRecord& record,
                        const programs::Catalog& catalog,
                        std::uint64_t nowTicks) noexcept {
    auto& state = record.matchState;
    if (state.aborted || !state.hasStarted || state.revision == UINT32_MAX) {
        return;
    }
    const auto end = state.endReason == EndReason::none ? nowTicks : state.endedAtTicks;
    if (end <= state.startedAtTicks) {
        return;
    }
    const auto elapsed = end - state.startedAtTicks;
    constexpr auto framesPerSecond = activity_clock::kSimulationFramesPerSecond;
    const auto elapsedFrames = elapsed / activity_clock::kTicksPerSecond * framesPerSecond
                               + elapsed % activity_clock::kTicksPerSecond * framesPerSecond
                                     / activity_clock::kTicksPerSecond;
    if (elapsedFrames <= state.statisticFrames) {
        return;
    }
    const auto frames = elapsedFrames - state.statisticFrames > kCatchUpFrames
                            ? state.statisticFrames + kCatchUpFrames
                            : elapsedFrames;
    std::array<bool, programs::kPlayerStatisticCapacity> timed{}, timeInputs{};
    if (!time_columns(catalog, timed, timeInputs)) {
        return;
    }
    auto statistics = state.statistics;
    bool changed = false;
    // Execute the installed recurrences unchanged. Recipient count and publication rate
    // cannot add frames; the allocation owns one monotonic cursor shared by all readers.
    for (auto& player : statistics) {
        if (find_member(record, player.playerKey, true) == nullptr) {
            continue;
        }
        for (auto frame = state.statisticFrames; frame < frames; ++frame) {
            bool frameChanged = false;
            for (std::size_t p = 0; p < catalog.computedCount; ++p) {
                const auto& program = catalog.computed[p];
                if (team_only(catalog, program)) {
                    continue;
                }
                if (timed[column(catalog, program.outputKey)]
                    && !evaluate(catalog, program, player, frameChanged)) {
                    return;
                }
            }
            changed = changed || frameChanged;
            // The same deterministic programs and inputs have reached a fixed point.
            if (!frameChanged) {
                break;
            }
        }
    }
    state.statistics = statistics;
    state.statisticFrames = frames;
    if (changed) {
        // Counted apart, so publication can tell a frame of the time recurrences from an event.
        const auto before = state.revision;
        ++state.revision;
        ++state.timedRevisions;
        carry_projection(state, before);
    }
}

void observe_named_statistics(
    std::uint64_t sessionId,
    std::uint64_t memberKey,
    const middleware::bap::activity_message::sensor_sense_update::Update& update) noexcept {
    // BF6380 produces these inputs. The installed program owns their total and display.
    constexpr std::array<std::uint32_t, kNamedStatisticCount> keys{kDamageDealtStatistic,
                                                                   kSecondNamedStatistic};
    State retained{};
    build_data::activity_rules::Rules rules{};
    if (sessionId == 0 || memberKey == 0 || update.participationCount > update.participation.size()
        || !read(sessionId, retained, &rules) || retained.endReason != EndReason::none
        || rules.statistics == nullptr) {
        return;
    }
    const auto& catalog = *rules.statistics;
    std::array<bool, programs::kPlayerStatisticCapacity> timed{}, timeInputs{};
    if (!time_columns(catalog, timed, timeInputs)) {
        return;
    }
    std::array<std::size_t, kNamedStatisticCount> columns{column(catalog, keys[0]),
                                                          column(catalog, keys[1])};
    for (auto index : columns) {
        if (index >= catalog.playerColumnCount) {
            return;
        }
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        auto& state = record.matchState;
        const auto* identity = find_member(record, memberKey, false);
        if (record.createdRevision == retained.allocationRevision
            && record.activityClock.initialized && !state.aborted
            && state.endReason == EndReason::none && state.revision != UINT32_MAX
            && identity != nullptr && identity->hasIdentity && identity->hasAssignedPartition
            && identity->identity.opaqueSoid != 0) {
            for (auto& bound : state.introMembers) {
                if (bound.memberKey != memberKey
                    || bound.joinIdentity != identity->identity.joinIdentity) {
                    continue;
                }
                for (std::size_t i = 0; i < update.participationCount; ++i) {
                    if (state.revision == UINT32_MAX) {
                        break;
                    }
                    const auto& observation = update.participation[i];
                    if (observation.key != bound.group || observation.slotIndex != bound.slot
                        || observation.partition != identity->assignedPartition
                        || observation.valueCount > observation.values.size()) {
                        continue;
                    }
                    std::array<float, kNamedStatisticCount> inputs{};
                    bool valid = true;
                    for (std::size_t v = 0; v < observation.valueCount; ++v) {
                        const auto& value = observation.values[v];
                        for (std::size_t prior = 0; prior < v; ++prior) {
                            valid = valid && observation.values[prior].nameHash != value.nameHash;
                        }
                        for (std::size_t k = 0; k < keys.size(); ++k) {
                            if (value.nameHash == keys[k]) {
                                inputs[k] = std::bit_cast<float>(value.floatBits);
                                valid = valid && std::isfinite(inputs[k]);
                            }
                        }
                    }
                    if (!valid) {
                        continue;
                    }
                    if (!state.hasStarted) {
                        bound.namedBaseline = inputs;
                        continue;
                    }
                    auto statistics = state.statistics;
                    PlayerStatistics* player = nullptr;
                    for (auto& candidate : statistics) {
                        if (candidate.playerKey == identity->identity.opaqueSoid) {
                            player = &candidate;
                            break;
                        }
                        if (player == nullptr && candidate.playerKey == 0) {
                            player = &candidate;
                        }
                    }
                    if (player == nullptr) {
                        continue;
                    }
                    bool changed = false;
                    for (std::size_t k = 0; k < keys.size(); ++k) {
                        const float value = inputs[k] - bound.namedBaseline[k];
                        valid = valid && std::isfinite(value);
                        changed = changed
                                  || std::bit_cast<std::uint32_t>(player->values[columns[k]])
                                         != std::bit_cast<std::uint32_t>(value);
                        player->values[columns[k]] = value;
                    }
                    if (!valid || !changed) {
                        continue;
                    }
                    player->playerKey = identity->identity.opaqueSoid;
                    for (std::size_t p = 0; valid && p < catalog.computedCount; ++p) {
                        const auto& program = catalog.computed[p];
                        if (team_only(catalog, program)) {
                            continue;
                        }
                        if (!timed[column(catalog, program.outputKey)]) {
                            valid = evaluate(catalog, program, *player, changed);
                        }
                    }
                    if (valid) {
                        state.statistics = statistics;
                        const auto before = state.revision;
                        ++state.revision;
                        carry_projection(state, before);
                    }
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

void observe_incident(std::uint64_t sessionId,
                      std::uint64_t senderMemberKey,
                      std::span<const std::byte> canonical,
                      std::uint64_t nowMilliseconds) noexcept {
    State retained{};
    build_data::activity_rules::Rules rules{};
    if (sessionId == 0 || senderMemberKey == 0 || !read(sessionId, retained, &rules)
        || retained.endReason != EndReason::none || rules.statistics == nullptr
        || rules.incidentTargets == nullptr) {
        return;
    }
    incident::Incident envelope{};
    targets::Selection selection{};
    std::array<std::byte, incident::kPayloadMaximum> body{};
    incident::PlayerRoles roles{};
    if (!incident::fully_framed(canonical, envelope)
        || !incident::resolve_targets(canonical, *rules.incidentTargets, selection)
        || !incident::extract_body(canonical, envelope, body)
        || !incident::read_player_roles(std::span(body).first(envelope.payloadLength),
                                        rules.incidentTargets->targets[envelope.primaryTarget].type,
                                        roles)) {
        return;
    }
    bool changed = false;
    bool accepted = false;
    std::uint32_t revision{};
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        auto& state = record.matchState;
        const auto* sender = find_member(record, senderMemberKey, false);
        if (record.createdRevision == retained.allocationRevision
            && record.activityClock.initialized && !state.aborted
            && state.endReason == EndReason::none && state.revision != UINT32_MAX
            && sender != nullptr && sender->identity.opaqueSoid == roles.sourcePlayer) {
            Sequence* sequence = nullptr;
            for (auto& row : state.statisticSequences) {
                if (row.sourcePlayer == roles.sourcePlayer) {
                    sequence = &row;
                    break;
                }
                if (sequence == nullptr && row.sourcePlayer == 0) {
                    sequence = &row;
                }
            }
            if (sequence != nullptr) {
                auto next = *sequence;
                auto statistics = state.statistics;
                if (accept_sequence(next, roles.sequence)
                    && apply(record,
                             *rules.statistics,
                             *rules.incidentTargets,
                             selection,
                             roles,
                             statistics,
                             changed)) {
                    next.sourcePlayer = roles.sourcePlayer;
                    *sequence = next;
                    state.statistics = statistics;
                    accepted = true;
                    if (changed) {
                        const auto before = state.revision;
                        ++state.revision;
                        carry_projection(state, before);
                    }
                    revision = state.revision;
                } else {
                    changed = false;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    const auto type = rules.incidentTargets->targets[envelope.primaryTarget].type;
    if (accepted && type == targets::kKillType) {
        // The source already classified the completed event. Only the native qualifying
        // kill key can enter the installed activity score policy; a raw kill report alone
        // is not enough. The shared ingestion window has accepted the report exactly once.
        // Every selected target key travels with the kill as the client's classification.
        std::array<std::uint32_t, std::tuple_size_v<decltype(selection.targets)>> classification{};
        bool qualifies = false;
        for (std::size_t i = 0; i < selection.count; ++i) {
            classification[i] = rules.incidentTargets->targets[selection.targets[i]].key;
            qualifies = qualifies || classification[i] == rules.mode->qualifyingKillKey;
        }
        incident::KillFacts facts{};
        if (qualifies
            && incident::read_kill_facts(std::span(body).first(envelope.payloadLength), facts)) {
            observe_kill(sessionId,
                         senderMemberKey,
                         facts,
                         nowMilliseconds,
                         std::span(classification).first(selection.count));
        }
    }
    if (accepted && type == targets::kCaptureType && rules.mode->zones.count != 0) {
        // The simulating peer's capture component completed the zone report; the zone
        // transaction applies it once, under the kill replay window.
        std::array<std::uint32_t, std::tuple_size_v<decltype(selection.targets)>> classification{};
        for (std::size_t i = 0; i < selection.count; ++i) {
            classification[i] = rules.incidentTargets->targets[selection.targets[i]].key;
        }
        incident::CaptureFacts facts{};
        if (incident::read_capture_facts(std::span(body).first(envelope.payloadLength), facts)) {
            observe_capture(sessionId,
                            senderMemberKey,
                            facts,
                            nowMilliseconds,
                            std::span(classification).first(selection.count));
        }
    }
    if (changed) {
        core::log::writef(core::log::Channel::state,
                          core::log::Level::debug,
                          "ev=activity_match stage=statistics session=0x%016llX "
                          "source=0x%016llX sequence=%u events=%u revision=%u",
                          static_cast<unsigned long long>(sessionId),
                          static_cast<unsigned long long>(roles.sourcePlayer),
                          roles.sequence,
                          selection.count,
                          revision);
    }
}

} // namespace sunrise::state::activity::match_state

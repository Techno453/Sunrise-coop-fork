#include <Windows.h>

#include <array>
#include <limits>

#include "../../../core/logging/log.h"
#include "../../../middleware/bap/activity_message/capture_zone_auth.h"
#include "../../../middleware/bap/activity_message/scoreboard_record.h"
#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "../../build_data/activity_options/runtime.h"
#include "../../build_data/activity_rules/runtime.h"
#include "../../runtime/storage/internal.h"
#include "../activity_clock/definition.h"
#include "../transactions/internal.h"
#include "definition.h"
#include "member.h"
#include "runtime.h"
#include "script_policy.h"

namespace sunrise::state::activity::match_state {
namespace {
namespace keys = middleware::content::packages::tables::activity_settings;
bool resolve_script_definition(SessionRecord&,
                               std::size_t,
                               const build_data::activity_rules::Rules&,
                               script_policy::ModeDefinition&) noexcept;
const membership::MembershipState*
member(const SessionRecord& record, std::uint64_t key, bool player) noexcept {
    return current_member(record, key, player);
}

std::int32_t
selected_limit(const SessionRecord& record, std::uint32_t key, std::int32_t fallback) noexcept {
    const auto& selection = record.destination;
    if (!selection.hasOptionPairs) {
        return fallback;
    }
    if (selection.optionPairCount > selection.optionPairs.size()) {
        return 0;
    }
    std::int32_t value{};
    return build_data::activity_options::resolve(
               std::span(selection.optionPairs).first(selection.optionPairCount), key, value)
               ? value
               : fallback;
}

/**
 * Seconds of match time: the selected private time option, else the mode row's installed
 * `time_limit`, which a private match left at its default time plays.
 */
std::int32_t match_seconds(const SessionRecord& record,
                           const build_data::activity_rules::Rules& rules) noexcept {
    return selected_limit(record, keys::kTimeLimitKey, rules.settings.timeLimitSeconds);
}

bool expire(SessionRecord& record,
            std::size_t slot,
            const build_data::activity_rules::Rules& rules,
            std::uint64_t now,
            std::uint64_t limit) noexcept {
    auto& state = record.matchState;
    if (state.endReason == EndReason::none) {
        match_modes::TimeDecision decision{};
        {
            script_policy::Request request{};
            request.kind = script_policy::Kind::time;
            request.time = {
                state.hasStarted && now >= state.startedAtTicks ? now - state.startedAtTicks : 0,
                limit,
                state.hasStarted,
                false};
            script_policy::Response response{};
            if (!evaluate_script(record, slot, *rules.mode, request, response)) {
                return false;
            }
            decision = {response.time.finish, state.startedAtTicks + response.time.elapsedAtEnd};
        }
        if (decision.finish && state.hasStarted && decision.endedAtTicks >= state.startedAtTicks
            && decision.endedAtTicks <= now) {
            state.endReason = EndReason::timeLimit;
            state.endedAtTicks = decision.endedAtTicks;
            // The ending moves no revision, so the lifecycle projection is marked stale instead.
            state.projected = false;
        }
    }
    return state.endReason != EndReason::none;
}

/** Script selects lifecycle policy; these flags are compatibility projections for publication. */
bool project_flow(SessionRecord& record,
                  std::size_t slot,
                  const build_data::activity_rules::Rules& rules,
                  std::uint64_t now,
                  script_policy::FlowDecision& flow,
                  bool& postgame,
                  bool& advance) noexcept {
    const auto& state = record.matchState;

    script_policy::Request request{};
    request.kind = script_policy::Kind::flow;
    request.flow = {state.endReason != EndReason::none && now >= state.endedAtTicks
                        ? now - state.endedAtTicks
                        : 0,
                    activity_clock::kTicksPerSecond,
                    rules.settings.completeSeconds,
                    rules.settings.postgameSeconds,
                    state.hasStarted,
                    state.endReason != EndReason::none,
                    intro_active(state)};
    script_policy::Response response{};
    if (!evaluate_script(record, slot, *rules.mode, request, response)) {
        return false;
    }
    flow = response.flow;
    postgame =
        flow.phase == script_policy::Phase::postgame || flow.phase == script_policy::Phase::advance;
    advance = flow.phase == script_policy::Phase::advance;
    return true;
}

/** Native type of an authority score notice. */
constexpr std::int8_t kNoticeType =
    middleware::content::packages::tables::incident_targets::kAmountType;
/** Sentinel outside the installed target index range. */
constexpr std::uint16_t kNoNotice = (std::numeric_limits<std::uint16_t>::max)();

/** Resolve an installed notice key or retain the caller's fallback. */
std::uint16_t notice_target(const build_data::activity_rules::Rules& rules,
                            std::uint32_t key,
                            std::uint16_t fallback) noexcept {
    const auto* catalog = rules.incidentTargets;
    if (key == 0 || catalog == nullptr || catalog->targetCount > catalog->targets.size()) {
        return fallback;
    }
    for (std::uint16_t i = 0; i < catalog->targetCount; ++i) {
        if (catalog->targets[i].key == key && catalog->targets[i].type == kNoticeType) {
            return i;
        }
    }
    return fallback;
}

/** No substitution for a missing scripted binding. Resolve before any score/replay mutation. */
bool script_notice(const build_data::activity_rules::Rules& rules,
                   script_policy::Notice name,
                   std::uint16_t& target) noexcept {
    if (name == script_policy::Notice::none) {
        target = kNoNotice;
        return true;
    }
    if (name == script_policy::Notice::installed && rules.mode->scriptedNoticeBindings.empty()) {
        target = rules.scoreNoticeTarget;
        return true;
    }
    for (const auto& binding : rules.mode->scriptedNoticeBindings) {
        if (binding.name == name) {
            target = notice_target(rules, binding.key, kNoNotice);
            return target != kNoNotice;
        }
    }
    return false;
}

/** Copy admitted native classification without silently truncating evidence. */
bool copy_classification(std::span<const std::uint32_t> input,
                         script_policy::Classification& output) noexcept {
    if (input.size() > output.keys.size()) {
        return false;
    }
    std::copy(input.begin(), input.end(), output.keys.begin());
    output.count = input.size();
    return true;
}

/** Resolve the captured declaration under the existing State lock. */
bool resolve_script_definition(SessionRecord& record,
                               std::size_t slot,
                               const build_data::activity_rules::Rules& rules,
                               script_policy::ModeDefinition& output) noexcept {
    namespace policy = script_policy;
    policy::Request request{};
    request.kind = policy::Kind::definition;
    policy::Response response{};
    if (!evaluate_script(record, slot, *rules.mode, request, response)) {
        return false;
    }
    const auto& mode = response.definition;
    const bool factions =
        mode.factions == policy::FactionPolicy::freeForAll
            ? rules.settings.isFfa == 1 && rules.mode->teamCount == 0
            : rules.settings.isFfa == 0 && mode.teamCount == rules.mode->teamCount;
    constexpr std::uint8_t kControlZones = 3;
    if (!policy::supported(mode) || !factions || rules.settings.roundLimit != 1
        || (mode.objectives == policy::Objective::controlZones
            && rules.mode->zones.count != kControlZones)
        || (mode.hud == policy::Hud::control
            && rules.mode->objectiveHud != match_modes::objective_hud::kControl)) {
        abort_script(record, slot, rules.mode->key, "definition_contract");
        return false;
    }
    output = mode;
    return true;
}

/** The cached mode declaration of this allocation; the script is asked only the first time. */
bool definition_for(SessionRecord& record,
                    std::size_t slot,
                    const build_data::activity_rules::Rules& rules,
                    script_policy::ModeDefinition& output) noexcept {
    auto& state = record.matchState;
    if (state.aborted) {
        return false;
    }
    if (state.definitionAllocation != record.createdRevision) {
        script_policy::ModeDefinition definition{};
        if (!resolve_script_definition(record, slot, rules, definition)) {
            return false;
        }
        state.definition = definition;
        state.definitionAllocation = record.createdRevision;
    }
    output = state.definition;
    return true;
}

/** Projects the lifecycle and the mode declaration at `now` and stamps the projection. */
void project(SessionRecord& record,
             std::size_t slot,
             const build_data::activity_rules::Rules& rules,
             std::uint64_t now) noexcept {
    auto& state = record.matchState;
    script_policy::ModeDefinition definition{};
    script_policy::FlowDecision flow{};
    bool postgame = false;
    bool advance = false;
    const bool ready = definition_for(record, slot, rules, definition)
                       && project_flow(record, slot, rules, now, flow, postgame, advance);
    state.scriptFlowReady = ready && !state.aborted;
    if (state.scriptFlowReady) {
        state.scriptFlow = flow;
        state.postgameReady = postgame;
        state.advanceReady = advance;
    }
    state.projected = true;
    state.projectedRevision = state.revision;
    state.projectedTicks = now;
    state.projectedAllocation = record.createdRevision;
}

/**
 * The time-driven projection: the intro, the match clock's expiry, the time recurrences and the
 * lifecycle, at most once per `kProjectionIntervalMilliseconds` for one allocation. A call inside
 * that interval projects only a lifecycle that is not current.
 * @return The intro step taken, reported once State's lock is released.
 */
IntroReport refresh(SessionRecord& record,
                    std::size_t slot,
                    const build_data::activity_rules::Rules& rules,
                    std::uint64_t nowMilliseconds) noexcept {
    auto& state = record.matchState;
    const auto now = activity_clock::advance(record.activityClock, nowMilliseconds);
    const bool due =
        !state.refreshed || nowMilliseconds < state.refreshedAtMilliseconds
        || nowMilliseconds - state.refreshedAtMilliseconds >= kProjectionIntervalMilliseconds;
    IntroReport intro{};
    if (due) {
        state.refreshed = true;
        state.refreshedAtMilliseconds = nowMilliseconds;
        intro = advance_intro(record, slot, rules, now);
        const auto seconds = match_seconds(record, rules);
        if (seconds > 0) {
            static_cast<void>(
                expire(record,
                       slot,
                       rules,
                       now,
                       static_cast<std::uint64_t>(seconds) * activity_clock::kTicksPerSecond));
        }
        if (rules.statistics != nullptr) {
            advance_statistics(record, *rules.statistics, now);
        }
        project(record, slot, rules, now);
    } else {
        project_lifecycle(record, slot, rules, now);
    }
    return intro;
}

bool script_credit(const script_policy::ModeDefinition& mode,
                   script_policy::Credit& credit) noexcept {
    if (credit == script_policy::Credit::installed) {
        if (mode.factions != script_policy::FactionPolicy::freeForAll) {
            return false;
        }
        credit = script_policy::Credit::player;
    }
    return credit < script_policy::Credit::count
           && (!script_policy::credits_team(credit) || mode.teamScoring);
}

std::int32_t highest_team_score(const State& state) noexcept {
    std::int32_t highest{};
    for (const auto& row : state.teamScores) {
        if (row.partition >= 0 && row.value > highest) {
            highest = row.value;
        }
    }
    return highest;
}

/** Appends one notice, dropping the oldest when the retained list is full. */
void push_notice(State& state, const Notice& notice) noexcept {
    if (state.noticeCount == state.notices.size()) {
        for (std::size_t i = 1; i < state.notices.size(); ++i) {
            state.notices[i - 1] = state.notices[i];
        }
        --state.noticeCount;
        if (state.droppedNotices != (std::numeric_limits<std::uint32_t>::max)()) {
            ++state.droppedNotices;
        }
    }
    state.notices[state.noticeCount++] = notice;
}

/** @return The retained score row of `player`, or the first unused row not in `reserved`. */
Score* score_row(State& state, std::uint64_t player, std::span<Score* const> reserved) noexcept {
    Score* row = nullptr;
    for (auto& candidate : state.scores) {
        if (candidate.playerKey == player) {
            return &candidate;
        }
        bool taken = false;
        for (const auto* used : reserved) {
            taken = taken || used == &candidate;
        }
        if (row == nullptr && candidate.playerKey == 0 && !taken) {
            row = &candidate;
        }
    }
    return row;
}

/** @return The retained row of `partition`, or the first unused row. */
TeamScore* team_row(State& state, std::int8_t partition) noexcept {
    TeamScore* row = nullptr;
    for (auto& candidate : state.teamScores) {
        if (candidate.partition == partition) {
            return &candidate;
        }
        if (row == nullptr && candidate.partition < 0) {
            row = &candidate;
        }
    }
    return row;
}

/** @return The retained row of `zone`, or the first unused row. */
ZoneHold* zone_row(State& state, std::uint32_t zone, std::int8_t placement) noexcept {
    ZoneHold* row = nullptr;
    for (auto& candidate : state.zones) {
        if (candidate.zone == zone) {
            return &candidate;
        }
    }
    // A new simulating peer may report the same placement under a new handle.
    for (auto& candidate : state.zones) {
        if (placement >= 0 && candidate.placement == placement) {
            return &candidate;
        }
        if (row == nullptr && candidate.zone == 0 && candidate.placement < 0) {
            row = &candidate;
        }
    }
    return row;
}

/** @return The authored zone whose unique capture event the report selected, or -1. */
std::int8_t zone_placement(const match_modes::Zones& zones,
                           std::span<const std::uint32_t> classification) noexcept {
    for (std::size_t i = 0; i < zones.count && i < zones.placements.size(); ++i) {
        for (const auto key : classification) {
            if (key == zones.placements[i].captureKey) {
                return static_cast<std::int8_t>(i);
            }
        }
    }
    return -1;
}

/** @return The source's replay window, or the first unused one. */
Sequence* source_sequence(State& state, std::uint64_t sourcePlayer) noexcept {
    Sequence* row = nullptr;
    for (auto& candidate : state.sequences) {
        if (candidate.sourcePlayer == sourcePlayer) {
            return &candidate;
        }
        if (row == nullptr && candidate.sourcePlayer == 0) {
            row = &candidate;
        }
    }
    return row;
}

/**
 * Resolves the partition a zone report credits. The capture component (0xED5070) counts every
 * occupant but writes entries only for occupants of the progressing faction, so the trailing
 * entries stay unwritten (key 0) and are skipped. Every written participant carries one
 * faction, which is a captured zone's new owner, and at least one must be a current member of
 * that partition. A report without participants (0xED7AD0) only moves ownership and credits
 * nobody.
 * @return False when the report credits nobody; its owner change stands regardless.
 */
bool capture_partition(const SessionRecord& record,
                       const middleware::bap::activity_message::incident::CaptureFacts& facts,
                       std::int8_t& partition) noexcept {
    namespace incident = middleware::bap::activity_message::incident;
    partition = -1;
    bool member = false;
    for (std::size_t i = 0; i < facts.count; ++i) {
        if (facts.players[i] == 0) {
            continue;
        }
        const auto faction = facts.factions[i];
        if (faction < 0 || (partition >= 0 && faction != partition)) {
            return false;
        }
        partition = faction;
        const auto* participant = current_member(record, facts.players[i], true);
        member = member
                 || (participant != nullptr && participant->hasAssignedPartition
                     && participant->assignedPartition == faction);
    }
    return member
           && (facts.capturingFaction == partition
               || facts.capturingFaction == incident::kNeutralFaction);
}
} // namespace

bool project_members(std::uint64_t sessionId,
                     std::uint64_t allocation,
                     std::span<const std::uint64_t> players,
                     std::span<MemberProjection> output) noexcept {
    if (players.size() > kPlayerCapacity || output.size() != players.size()) {
        return false;
    }
    std::array<MemberProjection, kPlayerCapacity> candidate{};
    bool ready = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    build_data::activity_rules::Rules rules{};
    if (slot != kInvalidSessionSlot
        && (allocation == 0 || activity.sessions[slot].createdRevision == allocation)
        && !activity.sessions[slot].matchState.aborted
        && resolve_rules(activity.sessions[slot], rules)) {
        auto& record = activity.sessions[slot];
        ready = true;
        for (std::size_t i = 0; ready && i < players.size(); ++i) {
            if (const auto* member = current_member(record, players[i], true);
                member != nullptr && member->hasAssignedPartition) {
                candidate[i].assigned = true;
                candidate[i].partition = member->assignedPartition;
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (ready) {
        std::copy_n(candidate.begin(), output.size(), output.begin());
    }
    return ready;
}

std::int16_t match_activity(const destination::DestinationSelection& destination) noexcept {
    return destination.sourceActivityIndex >= 0 ? destination.sourceActivityIndex
                                                : destination.activityIndex;
}

bool assignment(std::uint64_t sessionId, std::uint64_t player, std::int8_t& partition) noexcept {
    bool found = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        const auto* value = current_member(activity.sessions[slot], player, true);
        if (value && value->hasAssignedPartition) {
            partition = value->assignedPartition;
            found = true;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return found;
}

void observe_kill(std::uint64_t sessionId,
                  std::uint64_t senderMemberKey,
                  const middleware::bap::activity_message::incident::KillFacts& facts,
                  std::uint64_t nowMilliseconds,
                  std::span<const std::uint32_t> classification) noexcept {
    if (sessionId == 0 || senderMemberKey == 0 || facts.sourcePlayer == 0
        || facts.killingPlayer == 0 || facts.deadPlayer == 0) {
        return;
    }
    bool awarded = false;
    std::int32_t award{};
    std::int32_t total{};
    std::int8_t teamPartition = -1;
    std::int32_t teamTotal{};
    bool complete = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        build_data::activity_rules::Rules rules{};
        auto& state = record.matchState;
        const auto* sender = member(record, senderMemberKey, false);
        const auto* killer = member(record, facts.killingPlayer, true);
        const auto* victim = member(record, facts.deadPlayer, true);
        const match_modes::KillContext killContext{
            facts,
            killer != nullptr ? killer->assignedPartition : std::int8_t{-1},
            victim != nullptr ? victim->assignedPartition : std::int8_t{-1},
            classification,
            killer != nullptr ? zones_held(state, killer->assignedPartition) : std::uint8_t{0}};
        const bool eligible = record.activityClock.initialized && state.hasStarted && !state.aborted
                              && state.endReason == EndReason::none && resolve_rules(record, rules)
                              && sender != nullptr
                              && sender->identity.opaqueSoid == facts.sourcePlayer
                              && killer != nullptr && victim != nullptr && killer != victim
                              && killer->hasAssignedPartition && victim->hasAssignedPartition
                              && killer->assignedPartition >= 0 && victim->assignedPartition >= 0
                              && killer->assignedPartition == facts.killerFaction
                              && victim->assignedPartition == facts.victimFaction
                              && rules.mode->scriptedKillEligible(killContext) && award >= 0;
        if (eligible) {
            const auto now = activity_clock::advance(record.activityClock, nowMilliseconds);
            const auto seconds = match_seconds(record, rules);
            if (seconds > 0) {
                static_cast<void>(
                    expire(record,
                           slot,
                           rules,
                           now,
                           static_cast<std::uint64_t>(seconds) * activity_clock::kTicksPerSecond));
            }
            Sequence* sequence = nullptr;
            Score* score = nullptr;
            for (auto& row : state.sequences) {
                if (row.sourcePlayer == facts.sourcePlayer) {
                    sequence = &row;
                    break;
                }
                if (sequence == nullptr && row.sourcePlayer == 0) {
                    sequence = &row;
                }
            }
            for (auto& row : state.scores) {
                if (row.playerKey == facts.killingPlayer) {
                    score = &row;
                    break;
                }
                if (score == nullptr && row.playerKey == 0) {
                    score = &row;
                }
            }
            script_policy::ModeDefinition definition{};
            const bool definitionReady = definition_for(record, slot, rules, definition);
            const bool teamScoring = definition.teamScoring;
            TeamScore* team = nullptr;
            if (teamScoring) {
                for (auto& row : state.teamScores) {
                    if (row.partition == killer->assignedPartition) {
                        team = &row;
                        break;
                    }
                    if (team == nullptr && row.partition < 0) {
                        team = &row;
                    }
                }
            }
            const auto scoreLimit =
                selected_limit(record, keys::kScoreToWinKey, rules.settings.defaultScoreLimit);
            Sequence candidateSequence = sequence != nullptr ? *sequence : Sequence{};
            script_policy::Decision decision{award,
                                             false,
                                             script_policy::Notice::installed,
                                             teamScoring ? script_policy::Credit::both
                                                         : script_policy::Credit::player};
            std::uint16_t awardNotice = kNoNotice;
            bool decisionReady = definitionReady && state.endReason == EndReason::none
                                 && sequence != nullptr && score != nullptr && scoreLimit > 0
                                 && state.revision != (std::numeric_limits<std::uint32_t>::max)()
                                 && accept_sequence(candidateSequence, facts.sequence);
            if (decisionReady) {
                std::int32_t highestScore{};
                for (const auto& retained : state.scores) {
                    if (retained.playerKey != 0 && retained.value > highestScore) {
                        highestScore = retained.value;
                    }
                }
                script_policy::Request request{};
                request.kind = script_policy::Kind::kill;
                request.kill = {rules.settings.finalBlowScore,
                                score->value,
                                scoreLimit,
                                highestScore,
                                rules.settings.killScore,
                                team != nullptr ? team->value : 0,
                                highest_team_score(state),
                                rules.settings.advantageKillBonus,
                                rules.settings.powerPlayKillBonus,
                                killer->assignedPartition,
                                victim->assignedPartition,
                                definition.objectives != script_policy::Objective::none
                                    ? killContext.killerZones
                                    : std::uint8_t{0},
                                definition.objectives != script_policy::Objective::none
                                    ? rules.mode->zones.count
                                    : std::uint8_t{0},
                                teamScoring};
                if (!copy_classification(classification, request.kill.classification)) {
                    abort_script(record, slot, rules.mode->key, "kill_contract");
                    decisionReady = false;
                }
                script_policy::Response response{};
                decisionReady =
                    decisionReady && evaluate_script(record, slot, *rules.mode, request, response);
                if (decisionReady) {
                    decision = response.kill;
                    decisionReady = script_notice(rules, decision.notice, awardNotice)
                                    && script_credit(definition, decision.credit);
                    if (!decisionReady) {
                        abort_script(record, slot, rules.mode->key, "kill_contract");
                    }
                }
            }
            award = decision.points;
            if (decisionReady && award >= 0
                && (!script_policy::credits_player(decision.credit)
                    || award <= (std::numeric_limits<std::int32_t>::max)() - score->value)
                && (!script_policy::credits_team(decision.credit)
                    || (team != nullptr
                        && award <= (std::numeric_limits<std::int32_t>::max)() - team->value))
                && state.revision != (std::numeric_limits<std::uint32_t>::max)()) {
                *sequence = candidateSequence;
                sequence->sourcePlayer = facts.sourcePlayer;
                if (script_policy::credits_player(decision.credit)) {
                    score->playerKey = facts.killingPlayer;
                    score->value += award;
                }
                if (script_policy::credits_team(decision.credit)) {
                    team->partition = killer->assignedPartition;
                    team->value += award;
                    teamPartition = team->partition;
                    teamTotal = team->value;
                }
                ++state.revision;
                if (awardNotice != kNoNotice) {
                    push_notice(state,
                                {facts.killingPlayer,
                                 killer->identity.joinIdentity,
                                 state.revision,
                                 award,
                                 awardNotice});
                }
                total = score->value;
                awarded = true;
                if (decision.complete) {
                    state.endReason = EndReason::scoreLimit;
                    state.endedAtTicks = now;
                }
                complete = state.endReason != EndReason::none;
            }
            project_lifecycle(record, slot, rules, now);
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (awarded) {
        const auto level = complete ? core::log::Level::info : core::log::Level::debug;
        if (teamPartition < 0) {
            core::log::writef(core::log::Channel::state,
                              level,
                              "ev=activity_match stage=score session=0x%016llX source=0x%016llX "
                              "sequence=%u player=0x%016llX score=%d complete=%u",
                              static_cast<unsigned long long>(sessionId),
                              static_cast<unsigned long long>(facts.sourcePlayer),
                              facts.sequence,
                              static_cast<unsigned long long>(facts.killingPlayer),
                              total,
                              complete ? 1U : 0U);
        } else {
            core::log::writef(core::log::Channel::state,
                              level,
                              "ev=activity_match stage=score session=0x%016llX source=0x%016llX "
                              "sequence=%u player=0x%016llX score=%d team=%d team_score=%d "
                              "complete=%u",
                              static_cast<unsigned long long>(sessionId),
                              static_cast<unsigned long long>(facts.sourcePlayer),
                              facts.sequence,
                              static_cast<unsigned long long>(facts.killingPlayer),
                              total,
                              teamPartition,
                              teamTotal,
                              complete ? 1U : 0U);
        }
    }
}

void observe_capture(std::uint64_t sessionId,
                     std::uint64_t senderMemberKey,
                     const middleware::bap::activity_message::incident::CaptureFacts& facts,
                     std::uint64_t nowMilliseconds,
                     std::span<const std::uint32_t> classification) noexcept {
    namespace incident = middleware::bap::activity_message::incident;
    if (sessionId == 0 || senderMemberKey == 0 || facts.sourcePlayer == 0 || facts.zone == 0
        || facts.count > facts.players.size() || facts.count > facts.factions.size()
        || (facts.capturingFaction < 0 && facts.capturingFaction != incident::kNeutralFaction)) {
        return;
    }
    bool accepted = false;
    bool reverted = false;
    std::int8_t owner = -1;
    std::int8_t teamPartition = -1;
    std::int32_t teamTotal{};
    bool complete = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        build_data::activity_rules::Rules rules{};
        auto candidate = record.matchState;
        auto& state = candidate;
        const auto* sender = member(record, senderMemberKey, false);
        script_policy::ModeDefinition definition{};
        if (record.activityClock.initialized && resolve_rules(record, rules)
            && (definition_for(record, slot, rules, definition)
                && script_policy::has_capture_zones(definition.objectives))
            && sender != nullptr && sender->identity.opaqueSoid == facts.sourcePlayer) {
            const auto now = activity_clock::advance(record.activityClock, nowMilliseconds);
            const auto seconds = match_seconds(record, rules);
            if (seconds > 0) {
                static_cast<void>(
                    expire(record,
                           slot,
                           rules,
                           now,
                           static_cast<std::uint64_t>(seconds) * activity_clock::kTicksPerSecond));
            }
            state = record.matchState;
            // Only the award depends on who took part; the owner change does not.
            std::int8_t partition = -1;
            if (!capture_partition(record, facts, partition)) {
                partition = -1;
            }
            std::int32_t award{};
            const auto placement = zone_placement(rules.mode->zones, classification);
            const bool scores = partition >= 0;
            // Every participating member is credited once, however often the report names it.
            std::array<Score*, incident::kCaptureParticipantCapacity> credited{};
            std::array<std::uint64_t, std::tuple_size_v<decltype(credited)>> creditedPlayers{};
            std::size_t creditedCount = 0;
            bool rowsAvailable = true;
            std::int32_t maximumPlayerScore{};
            for (std::size_t i = 0; scores && i < facts.count; ++i) {
                const auto* participant = member(record, facts.players[i], true);
                if (participant == nullptr || !participant->hasAssignedPartition
                    || participant->assignedPartition != partition) {
                    continue;
                }
                bool repeated = false;
                for (std::size_t j = 0; j < creditedCount; ++j) {
                    repeated = repeated || creditedPlayers[j] == facts.players[i];
                }
                if (repeated) {
                    continue;
                }
                Score* row = score_row(state,
                                       facts.players[i],
                                       std::span<Score* const>(credited).first(creditedCount));
                if (row == nullptr
                    || award > (std::numeric_limits<std::int32_t>::max)() - row->value) {
                    rowsAvailable = false;
                } else {
                    if (row->value > maximumPlayerScore) {
                        maximumPlayerScore = row->value;
                    }
                    creditedPlayers[creditedCount] = facts.players[i];
                    credited[creditedCount++] = row;
                }
            }
            Sequence* sequence = source_sequence(state, facts.sourcePlayer);
            Sequence candidateSequence = sequence != nullptr ? *sequence : Sequence{};
            ZoneHold* zone = zone_row(state, facts.zone, placement);
            const std::int8_t newOwner = facts.capturingFaction >= 0 ? facts.capturingFaction : -1;
            TeamScore* team = scores ? team_row(state, partition) : nullptr;
            const auto scoreLimit =
                selected_limit(record, keys::kScoreToWinKey, rules.settings.defaultScoreLimit);
            script_policy::Decision decision{
                award, false, script_policy::Notice::none, script_policy::Credit::both};
            std::uint16_t notice = kNoNotice;
            // The simulating peer already holds the new owner: 0xED5070 tail-calls
            // 0xED7C20(zone, owner), and the zone's Auth only re-asserts an owner through a
            // newer owner revision (0xED7820 -> 0xED7AD0). A change the authority refuses
            // therefore republishes its own owner under a newer revision, which reverts the
            // simulator; a zone row no placement names is not published and is left alone.
            if (zone != nullptr && zone->owner != newOwner
                && zone->ownerRevision != (std::numeric_limits<std::int32_t>::max)()
                && state.revision != (std::numeric_limits<std::uint32_t>::max)()) {
                const bool running =
                    !state.aborted && state.hasStarted && state.endReason == EndReason::none;
                accepted = running && sequence != nullptr
                           && accept_sequence(candidateSequence, facts.sequence);
                if (accepted) {
                    script_policy::Request request{};
                    request.kind = script_policy::Kind::capture;
                    request.capture = {rules.settings.zoneCaptureScore,
                                       rules.settings.zoneNeutralizeScore,
                                       maximumPlayerScore,
                                       team != nullptr ? team->value : 0,
                                       highest_team_score(state),
                                       scoreLimit,
                                       partition,
                                       static_cast<std::uint8_t>(placement + 1),
                                       static_cast<std::uint8_t>(creditedCount),
                                       facts.capturingFaction == incident::kNeutralFaction,
                                       definition.teamScoring};
                    script_policy::Response response{};
                    accepted = evaluate_script(record, slot, *rules.mode, request, response);
                    if (accepted) {
                        decision = response.capture;
                        accepted = script_notice(rules, decision.notice, notice)
                                   && script_credit(definition, decision.credit);
                        if (!accepted) {
                            abort_script(record, slot, rules.mode->key, "capture_contract");
                        }
                        award = decision.points;
                    }
                    // Score capacity gates only the award below. A valid native owner change
                    // and its replay commit even when no score row remains.
                }
                // A finished match's revision stays frozen for its final-score delivery.
                reverted = !accepted && !record.matchState.aborted && !state.aborted
                           && state.endReason == EndReason::none
                           && (placement >= 0 || zone->placement >= 0);
                if (accepted || reverted) {
                    zone->zone = facts.zone;
                    zone->placement = placement >= 0 ? placement : zone->placement;
                    ++zone->ownerRevision;
                    ++state.revision;
                }
                if (accepted) {
                    *sequence = candidateSequence;
                    sequence->sourcePlayer = facts.sourcePlayer;
                    zone->owner = newOwner;
                }
                owner = zone->owner;
            }
            if (accepted && creditedCount > 0 && award > 0 && rowsAvailable && scoreLimit > 0
                && (!script_policy::credits_team(decision.credit)
                    || (team != nullptr
                        && award <= (std::numeric_limits<std::int32_t>::max)() - team->value))) {
                if (script_policy::credits_team(decision.credit)) {
                    team->partition = partition;
                    team->value += award;
                    teamPartition = team->partition;
                    teamTotal = team->value;
                }
                for (std::size_t i = 0; i < creditedCount; ++i) {
                    if (script_policy::credits_player(decision.credit)) {
                        credited[i]->playerKey = creditedPlayers[i];
                        credited[i]->value += award;
                    }
                    const auto* participant = member(record, creditedPlayers[i], true);
                    if (notice != kNoNotice && participant != nullptr) {
                        push_notice(state,
                                    {creditedPlayers[i],
                                     participant->identity.joinIdentity,
                                     state.revision,
                                     award,
                                     notice});
                    }
                }
                if (decision.complete) {
                    state.endReason = EndReason::scoreLimit;
                    state.endedAtTicks = now;
                }
            }
            if (accepted) {
                complete = state.endReason != EndReason::none;
            }
            if (!record.matchState.aborted) {
                record.matchState = candidate;
            } else {
                accepted = reverted = complete = false;
            }
            project_lifecycle(record, slot, rules, now);
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    if (accepted || reverted) {
        core::log::writef(core::log::Channel::state,
                          complete ? core::log::Level::info : core::log::Level::debug,
                          "ev=activity_match stage=zone session=0x%016llX source=0x%016llX "
                          "sequence=%u zone=0x%08X owner=%d team=%d team_score=%d complete=%u "
                          "applied=%u",
                          static_cast<unsigned long long>(sessionId),
                          static_cast<unsigned long long>(facts.sourcePlayer),
                          facts.sequence,
                          facts.zone,
                          owner,
                          teamPartition,
                          teamTotal,
                          complete ? 1U : 0U,
                          accepted ? 1U : 0U);
    }
}

void project_lifecycle(SessionRecord& record,
                       std::size_t slot,
                       const build_data::activity_rules::Rules& rules,
                       std::uint64_t nowTicks) noexcept {
    const auto& state = record.matchState;
    if (!state.projected || state.projectedAllocation != record.createdRevision
        || state.projectedRevision != state.revision) {
        project(record, slot, rules, nowTicks);
    }
}

bool publication(std::uint64_t sessionId,
                 std::uint64_t nowMilliseconds,
                 Publication& output) noexcept {
    bool found = false;
    IntroReport intro{};
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        build_data::activity_rules::Rules rules{};
        if (record.activityClock.initialized && resolve_rules(record, rules)) {
            intro = refresh(record, slot, rules, nowMilliseconds);
            const auto& state = record.matchState;
            output = {record.createdRevision,
                      state.revision,
                      state.endReason != EndReason::none,
                      state.advanceReady,
                      state.postgameReady,
                      rules.mode,
                      intro_active(state)};
            output.scriptFlow = state.scriptFlow;
            output.scriptFlowReady = state.scriptFlowReady;
            output.timedRevisions = state.timedRevisions;
            output.statisticFrames = state.statisticFrames;
            output.aborted = state.aborted;
            if (output.aborted) {
                output.finished = output.advanceReady = output.postgameReady = output.introActive =
                    false;
                output.scriptFlowReady = false;
            }
            found = output.scriptFlowReady || output.aborted;
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_intro(sessionId, intro);
    return found;
}

bool snapshot(std::uint64_t sessionId,
              State& output,
              build_data::activity_rules::Rules* settings) noexcept {
    bool found = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    build_data::activity_rules::Rules rules{};
    if (slot != kInvalidSessionSlot && resolve_rules(activity.sessions[slot], rules)) {
        auto& record = activity.sessions[slot];
        project_lifecycle(record, slot, rules, record.activityClock.lastTicks);
        output = record.matchState;
        output.allocationRevision = record.createdRevision;
        output.scriptFlowReady = output.scriptFlowReady && !output.aborted;
        if (settings != nullptr) {
            *settings = rules;
        }
        found = true;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return found;
}

bool read(std::uint64_t sessionId,
          State& output,
          build_data::activity_rules::Rules* settings) noexcept {
    bool found = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    build_data::activity_rules::Rules rules{};
    if (slot != kInvalidSessionSlot && resolve_rules(activity.sessions[slot], rules)) {
        output = activity.sessions[slot].matchState;
        output.allocationRevision = activity.sessions[slot].createdRevision;
        if (settings != nullptr) {
            *settings = rules;
        }
        found = true;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return found;
}

bool script_definition(std::uint64_t sessionId,
                       std::uint64_t allocationRevision,
                       script_policy::ModeDefinition& output) noexcept {
    bool success = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    build_data::activity_rules::Rules rules{};
    if (slot != kInvalidSessionSlot && activity.sessions[slot].createdRevision == allocationRevision
        && resolve_rules(activity.sessions[slot], rules)) {
        success = definition_for(activity.sessions[slot], slot, rules, output);
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return success;
}

bool project_script_results(
    std::uint64_t sessionId,
    std::uint64_t allocationRevision,
    middleware::bap::activity_message::scoreboard_record::Record& output) noexcept {
    if (output.elementCount > output.elements.size()
        || output.elementCount > script_policy::kResultCapacity) {
        return false;
    }
    script_policy::Request request{};
    request.kind = script_policy::Kind::results;
    request.results.count = output.elementCount;
    for (std::size_t index = 0; index < output.elementCount; ++index) {
        const auto& row = output.elements[index];
        request.results.rows[index] = {row.statistics.values[0], row.live && row.hasKey};
    }
    bool success = false;
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    build_data::activity_rules::Rules rules{};
    if (slot != kInvalidSessionSlot && activity.sessions[slot].createdRevision == allocationRevision
        && resolve_rules(activity.sessions[slot], rules)) {
        auto& record = activity.sessions[slot];
        auto& state = record.matchState;
        request.results.titlesAvailable = false;
        for (const auto title : rules.settings.placementTitles) {
            request.results.titlesAvailable = request.results.titlesAvailable || title >= 0;
        }
        if (state.resultsAllocation != record.createdRevision) {
            state.resultsAllocation = record.createdRevision;
            state.resultCount = state.nextResult = 0;
        }
        const ResultProjection* cached = nullptr;
        for (std::size_t index = 0; index < state.resultCount; ++index) {
            if (same_results(state.results[index].input, request.results)) {
                cached = &state.results[index];
            }
        }
        script_policy::Response response{};
        if (cached == nullptr && !state.aborted
            && evaluate_script(record, slot, *rules.mode, request, response)) {
            auto& entry = state.results[state.nextResult];
            entry = {request.results, response.results};
            state.nextResult =
                static_cast<std::uint8_t>((state.nextResult + 1) % state.results.size());
            if (state.resultCount < state.results.size()) {
                ++state.resultCount;
            }
            cached = &entry;
        }
        if (cached != nullptr && !state.aborted) {
            for (std::size_t index = 0; index < output.elementCount; ++index) {
                const auto& place = cached->decision.rows[index];
                output.elements[index].hudTitle =
                    place.place > 0 && place.place <= rules.settings.placementTitles.size()
                        ? (place.tied ? rules.settings.tiedPlacementTitles[place.place - 1]
                                      : rules.settings.placementTitles[place.place - 1])
                        : -1;
            }
            success = true;
        } else {
            abort_script(record, slot, rules.mode->key);
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return success;
}

bool pending_notices(std::uint64_t sessionId,
                     std::uint64_t memberKey,
                     NoticeBatch& output) noexcept {
    NoticeBatch batch{};
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        const auto& record = activity.sessions[slot];
        const auto* recipient = member(record, memberKey, false);
        if (recipient != nullptr) {
            batch.allocation = record.createdRevision;
            batch.memberKey = memberKey;
            for (std::size_t i = 0;
                 i < record.matchState.noticeCount && batch.count < batch.notices.size();
                 ++i) {
                const auto& notice = record.matchState.notices[i];
                if (notice.player == recipient->identity.opaqueSoid
                    && notice.joinIdentity == recipient->identity.joinIdentity) {
                    batch.notices[batch.count++] = notice;
                }
            }
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (batch.count == 0) {
        return false;
    }
    output = batch;
    return true;
}

void commit_notices(std::uint64_t sessionId,
                    const NoticeBatch& batch,
                    std::size_t amount) noexcept {
    if (amount == 0 || amount > batch.count || amount > batch.notices.size()) {
        return;
    }
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot) {
        auto& record = activity.sessions[slot];
        const auto* recipient = member(record, batch.memberKey, false);
        if (record.createdRevision == batch.allocation && recipient != nullptr) {
            auto& state = record.matchState;
            std::uint8_t kept{};
            for (std::size_t i = 0; i < state.noticeCount; ++i) {
                const auto notice = state.notices[i];
                bool delivered = false;
                if (notice.player == recipient->identity.opaqueSoid
                    && notice.joinIdentity == recipient->identity.joinIdentity) {
                    for (std::size_t j = 0; j < amount; ++j) {
                        const auto& offered = batch.notices[j];
                        delivered = delivered
                                    || (notice.sequence == offered.sequence
                                        && notice.player == offered.player
                                        && notice.joinIdentity == offered.joinIdentity);
                    }
                }
                if (!delivered) {
                    state.notices[kept++] = notice;
                }
            }
            for (std::size_t i = kept; i < state.noticeCount; ++i) {
                state.notices[i] = {};
            }
            state.noticeCount = kept;
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

void abort_script(SessionRecord& record,
                  std::size_t slot,
                  std::uint32_t modeKey,
                  std::string_view reason) noexcept {
    auto& state = record.matchState;
    if (state.aborted) {
        return;
    }
    state.aborted = true;
    script_policy::release(slot, record.sessionId, record.createdRevision);
    state.endReason = EndReason::none;
    state.endedAtTicks = 0;
    state.advanceReady = state.postgameReady = state.scriptFlowReady = false;
    state.noticeCount = 0;
    state.notices = {};
    state.cinematics = {};
    if (state.revision != (std::numeric_limits<std::uint32_t>::max)()) {
        ++state.revision;
    }
    script_policy::report_abort({slot, record.sessionId, record.createdRevision, modeKey}, reason);
}

bool evaluate_script(SessionRecord& record,
                     std::size_t slot,
                     const match_modes::Definition& mode,
                     const script_policy::Request& request,
                     script_policy::Response& response) noexcept {
    if (record.matchState.aborted) {
        return false;
    }
    if (script_policy::evaluate(
            {slot, record.sessionId, record.createdRevision, mode.key}, request, response)) {
        return true;
    }
    abort_script(record,
                 slot,
                 mode.key,
                 request.kind == script_policy::Kind::definition
                     ? "definition_callback"
                     : script_policy::kHandlers[static_cast<std::size_t>(request.kind)]);
    return false;
}

bool aborted(std::uint64_t sessionId, std::uint64_t& allocation) noexcept {
    bool failed = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& activity = runtime::storage::g_state.activity;
    const auto slot = transactions::find_session(activity, sessionId);
    if (slot != kInvalidSessionSlot && activity.sessions[slot].matchState.aborted) {
        failed = true;
        allocation = activity.sessions[slot].createdRevision;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return failed;
}

} // namespace sunrise::state::activity::match_state

#include <Windows.h>

#include <array>

#include "../../../core/logging/log.h"
#include "../../../middleware/bap/activity_message/incident.h"
#include "../../runtime/storage/internal.h"
#include "../transactions/internal.h"
#include "member.h"
#include "runtime.h"

namespace sunrise::state::activity::match_state {
namespace {
namespace incident = middleware::bap::activity_message::incident;
namespace cinematic = middleware::bap::activity_message::cinematic_record;
namespace targets = middleware::content::packages::tables::incident_targets;
/** A clip's command plays at generation 1; a stopped clip moves to generation 2. */
constexpr std::uint32_t kPlayingGeneration = 1;
constexpr std::uint32_t kStoppedGeneration = 2;
/** Native cinematic report events: playback started, and playback stopped or failed. */
constexpr std::uint32_t kCinematicStarted = 0xAECA3822U;
constexpr std::uint32_t kCinematicStopped = 0x3C03F059U;

const membership::MembershipState* member(const SessionRecord& record, std::uint64_t key) noexcept {
    return current_member(record, key);
}

IntroMember* binding(State& state, std::uint64_t key) noexcept {
    for (auto& row : state.introMembers) {
        if (row.memberKey == key) {
            return &row;
        }
    }
    return nullptr;
}

bool current(const SessionRecord& record, const IntroMember& row) noexcept {
    const auto* identity = member(record, row.memberKey);
    return identity != nullptr && identity->hasIdentity
           && identity->identity.joinIdentity == row.joinIdentity;
}

/** A current unready member holds the initial edge; departed members cannot hold playback. */
bool all_ready(SessionRecord& record) noexcept {
    bool present = false;
    for (std::size_t i = 0; i < kInvalidMemberRow; ++i) {
        const auto* identity = member_identity(record, i);
        if (!identity) {
            continue;
        }
        present = true;
        const auto* row = binding(record.matchState, identity->memberKey);
        if (!row || !current(record, *row) || !row->ready) {
            return false;
        }
    }
    return present;
}

bool next_clip(SessionRecord& record,
               std::size_t slot,
               const build_data::activity_rules::Rules& rules,
               std::uint64_t now) noexcept {
    auto& state = record.matchState;
    if (state.aborted || state.hasStarted || state.endReason != EndReason::none
        || state.introStep > state.cinematics.size() || state.revision == UINT32_MAX) {
        return false;
    }
    std::uint8_t nextStep;
    bool start = false;
    match_modes::IntroClip nextClip{};
    std::uint32_t usedClip{};
    {
        bool complete = true, present = false;
        for (const auto& participant : state.introMembers) {
            if (participant.ready && current(record, participant)) {
                present = true;
                complete = complete && participant.finished;
            }
        }
        script_policy::Request request{};
        request.kind = script_policy::Kind::intro;
        request.intro = {state.introStep,
                         static_cast<std::uint8_t>(state.cinematics.size()),
                         state.hasStarted,
                         state.endReason != EndReason::none,
                         all_ready(record),
                         complete && present,
                         state.scriptedIntroUsed};
        script_policy::Response response{};
        if (!evaluate_script(record, slot, *rules.mode, request, response)) {
            return false;
        }
        nextStep = response.intro.nextStep;
        start = response.intro.start;
        if (nextStep == state.introStep) {
            return false;
        }
        if (!start) {
            for (const auto& binding : rules.mode->scriptedIntroBindings) {
                if (binding.name == response.intro.clip) {
                    nextClip = binding.clip;
                }
            }
            if (nextClip.group == 0) {
                abort_script(record, slot, rules.mode->key);
                return false;
            }
            usedClip = 1U << static_cast<unsigned>(response.intro.clip);
        }
    }
    if (state.introStep != 0) {
        auto& previous = state.cinematics[state.introStep - 1];
        previous.active = false;
        previous.generation = kStoppedGeneration;
    }
    state.introStep = nextStep;
    state.scriptedIntroUsed |= usedClip;
    ++state.revision;
    if (start) {
        state.hasStarted = true;
        state.startedAtTicks = now;
        return true;
    }
    state.cinematics[state.introStep - 1] = {
        nextClip.group, kPlayingGeneration, nextClip.slot, true};
    for (auto& row : state.introMembers) {
        row.started = false;
        row.finished = false;
        row.failed = false;
        row.cinematicStartedAtMilliseconds = 0;
    }
    return true;
}

} // namespace

IntroReport advance_intro(SessionRecord& record,
                          std::size_t slot,
                          const build_data::activity_rules::Rules& rules,
                          std::uint64_t nowTicks) noexcept {
    IntroReport report{};
    const auto& state = record.matchState;
    for (const auto& participant : state.introMembers) {
        if (participant.ready && current(record, participant)) {
            report.failure = report.failure || participant.failed;
        }
    }
    report.taken = next_clip(record, slot, rules, nowTicks);
    report.step = state.introStep;
    report.started = state.hasStarted;
    report.startedAtTicks = state.startedAtTicks;
    return report;
}

void report_intro(std::uint64_t sessionId, const IntroReport& report) noexcept {
    if (!report.taken) {
        return;
    }
    core::log::writef(core::log::Channel::state,
                      report.failure ? core::log::Level::warn : core::log::Level::info,
                      "ev=activity_match stage=intro session=0x%016llX step=%u "
                      "started=%u start_ticks=%llu playback_failed=%u",
                      static_cast<unsigned long long>(sessionId),
                      report.step,
                      report.started ? 1U : 0U,
                      static_cast<unsigned long long>(report.startedAtTicks),
                      report.failure ? 1U : 0U);
}

void bind_intro_member(std::uint64_t sessionId,
                       std::uint64_t memberKey,
                       std::uint32_t group,
                       std::uint16_t slot) noexcept {
    if (sessionId == 0 || memberKey == 0 || group == 0) {
        return;
    }
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto index = transactions::find_session(activity, sessionId);
    if (index != kInvalidSessionSlot) {
        auto& record = activity.sessions[index];
        build_data::activity_rules::Rules rules{};
        const auto* identity = member(record, memberKey);
        if (record.matchState.endReason == EndReason::none && identity != nullptr
            && identity->hasIdentity && resolve_rules(record, rules)) {
            auto* row = binding(record.matchState, memberKey);
            if (row == nullptr) {
                for (auto& candidate : record.matchState.introMembers) {
                    if (!current(record, candidate)) {
                        row = &candidate;
                        break;
                    }
                }
            }
            if (row != nullptr
                && (row->memberKey != memberKey
                    || row->joinIdentity != identity->identity.joinIdentity || row->group != group
                    || row->slot != slot)) {
                *row = {};
                row->memberKey = memberKey;
                row->joinIdentity = identity->identity.joinIdentity;
                row->group = group;
                row->slot = slot;
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

void observe_participation(
    std::uint64_t sessionId,
    std::uint64_t memberKey,
    const middleware::bap::activity_message::sensor_sense_update::Update& update,
    std::uint64_t nowMilliseconds) noexcept {
    if (sessionId == 0 || memberKey == 0
        || update.participationCount > update.participation.size()) {
        return;
    }
    IntroReport intro{};
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto index = transactions::find_session(activity, sessionId);
    if (index != kInvalidSessionSlot) {
        auto& record = activity.sessions[index];
        auto& state = record.matchState;
        auto* row = binding(state, memberKey);
        const auto* identity = member(record, memberKey);
        build_data::activity_rules::Rules rules{};
        if (state.introStep == 0 && row != nullptr && current(record, *row)
            && resolve_rules(record, rules) && identity != nullptr && identity->hasAssignedPartition
            && record.activityClock.initialized) {
            // Only a member becoming ready is a decision for the script; a repeated report is not.
            bool readied = false;
            for (std::size_t i = 0; i < update.participationCount; ++i) {
                const auto& sensed = update.participation[i];
                if (!row->ready && sensed.key == row->group && sensed.slotIndex == row->slot
                    && sensed.partition == identity->assignedPartition && sensed.regionReady
                    && sensed.loadingComplete && sensed.partitionReady) {
                    row->ready = true;
                    readied = true;
                }
            }
            if (readied) {
                const auto now = activity_clock::advance(record.activityClock, nowMilliseconds);
                intro = advance_intro(record, index, rules, now);
                if (intro.taken) {
                    project_lifecycle(record, index, rules, now);
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_intro(sessionId, intro);
}

void observe_cinematic(std::uint64_t sessionId,
                       std::uint64_t memberKey,
                       std::span<const std::byte> canonical,
                       std::uint64_t nowMilliseconds) noexcept {
    State retained{};
    build_data::activity_rules::Rules rules{};
    if (sessionId == 0 || memberKey == 0 || !read(sessionId, retained, &rules)
        || retained.introStep == 0 || retained.introStep > retained.cinematics.size()
        || !retained.cinematics[retained.introStep - 1].active
        || rules.incidentTargets == nullptr) {
        return;
    }
    incident::Incident envelope{};
    middleware::content::packages::tables::incident_targets::Selection selection{};
    std::array<std::byte, incident::kPayloadMaximum> body{};
    incident::CinematicFacts facts{};
    if (!incident::fully_framed(canonical, envelope)
        || !incident::resolve_targets(canonical, *rules.incidentTargets, selection)
        || rules.incidentTargets->targets[envelope.primaryTarget].type != targets::kCinematicType
        || !incident::extract_body(canonical, envelope, body)
        || !incident::read_cinematic_facts(std::span(body).first(envelope.payloadLength), facts)) {
        return;
    }
    bool started = false, stopped = false;
    for (std::size_t i = 0; i < selection.count; ++i) {
        const auto key = rules.incidentTargets->targets[selection.targets[i]].key;
        started = started || key == kCinematicStarted;
        stopped = stopped || key == kCinematicStopped;
    }
    if (started == stopped || facts.type != cinematic::kSlotType) {
        return;
    }
    IntroReport intro{};
    const script_policy::DeferredScope deferred;
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& activity = runtime::storage::g_state.activity;
    const auto index = transactions::find_session(activity, sessionId);
    if (index != kInvalidSessionSlot) {
        auto& record = activity.sessions[index];
        auto& state = record.matchState;
        auto* row = binding(state, memberKey);
        const auto* identity = member(record, memberKey);
        if (record.createdRevision == retained.allocationRevision
            && state.introStep == retained.introStep && state.cinematics[state.introStep - 1].active
            && facts.slot == state.cinematics[state.introStep - 1].slot
            && facts.group == state.cinematics[state.introStep - 1].group && row != nullptr
            && current(record, *row) && row->ready && identity != nullptr
            && identity->identity.opaqueSoid == facts.sourcePlayer) {
            auto sequence = row->reports;
            if (!row->finished && accept_sequence(sequence, facts.sequence)
                && (!stopped || !row->started
                    || facts.timestampMilliseconds >= row->cinematicStartedAtMilliseconds)) {
                sequence.sourcePlayer = facts.sourcePlayer;
                row->reports = sequence;
                if (started) {
                    row->started = true;
                    row->cinematicStartedAtMilliseconds = facts.timestampMilliseconds;
                } else {
                    row->finished = true;
                    row->failed = !row->started;
                    const auto now = activity_clock::advance(record.activityClock, nowMilliseconds);
                    intro = advance_intro(record, index, rules, now);
                    if (intro.taken) {
                        project_lifecycle(record, index, rules, now);
                    }
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    report_intro(sessionId, intro);
}

} // namespace sunrise::state::activity::match_state

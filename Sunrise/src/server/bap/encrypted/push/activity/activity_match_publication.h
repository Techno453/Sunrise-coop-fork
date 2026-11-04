#pragma once

#include <cstdint>

#include "../../../../../state/activity/activity_clock/definition.h"
#include "../../../../../state/activity/match_modes/definition.h"
#include "../../../../../state/activity/match_state/definition.h"
#include "../../../internal.h"

namespace sunrise::server::bap::encrypted::push::activity {

/** The match facts one roster body carries. */
struct MatchFacts final {
    std::uint64_t bindingGeneration{};
    std::uint64_t allocation{};
    std::uint64_t statisticFrames{};
    std::uint32_t revision{};
    std::uint32_t timedRevisions{};
    std::uint8_t phase{};
    bool finalScores{};
    bool scores{};
    bool aborted{};
};

/** Commit only the match facts carried by a body the client holds, for the current binding. */
inline void commit_match_facts(Session& session, const MatchFacts& value) noexcept {
    if (value.bindingGeneration != session.activity.bindingGeneration) {
        return;
    }
    if (value.finalScores) {
        session.activityFinalScoresPublished = true;
        session.activityFinalScoreBindingGeneration = value.bindingGeneration;
        session.activityFinalScoreRevision = value.revision;
        session.activityFinalScoreAllocation = value.allocation;
    }
    if (value.scores || value.aborted) {
        session.activityMatchAborted = value.aborted;
        if (value.aborted) {
            session.activityFinalScoresPublished = false;
        }
        session.activityMatchPublished = true;
        session.activityMatchBindingGeneration = value.bindingGeneration;
        session.activityMatchAllocation = value.allocation;
        session.activityMatchRevision = value.revision;
        session.activityMatchTimedRevisions = value.timedRevisions;
        session.activityMatchStatisticFrames = value.statisticFrames;
        session.activityMatchPhase = value.phase;
    }
    if (value.finalScores || value.scores || value.aborted) {
        ++session.activityMatchCommits;
    }
}

/** Commit only the match facts carried by a delivered body for the current binding. */
inline void commit_match_publication(Session& session, const RosterPublication& value) noexcept {
    if (!value.staged) {
        return;
    }
    commit_match_facts(session,
                       {.bindingGeneration = value.bindingGeneration,
                        .allocation = value.matchAllocationRevision,
                        .statisticFrames = value.matchStatisticFrames,
                        .revision = value.matchRevision,
                        .timedRevisions = value.matchTimedRevisions,
                        .phase = value.matchPhase,
                        .finalScores = value.finalScores,
                        .scores = value.hasMatchScores,
                        .aborted = value.matchAborted});
}

/** An abort must not rewind a committed completion/postgame to active. */
[[nodiscard]] inline std::uint8_t match_abort_phase(const Session& session,
                                                    std::uint64_t allocation) noexcept {
    namespace match = state::activity::match_state;
    const bool same =
        session.activityMatchPublished
        && session.activityMatchBindingGeneration == session.activity.bindingGeneration
        && session.activityMatchAllocation == allocation;
    return same && session.activityMatchPhase > match::kActive ? session.activityMatchPhase
                                                               : match::kActive;
}

/**
 * Selects the lifetime this recipient is owed: final live scores, completion and postgame each
 * publish only after the one before it was delivered, ahead of native activity advancement.
 */
[[nodiscard]] inline std::uint8_t
match_publication_phase(const Session& session,
                        const state::activity::match_state::Publication& value) noexcept {
    namespace match = state::activity::match_state;
    if (value.mode == nullptr) {
        return match::kActive;
    }
    const auto& phases = value.mode->phases;
    if (value.aborted) {
        return match_abort_phase(session, value.allocation);
    }
    const bool finalDelivered =
        session.activityFinalScoresPublished
        && session.activityFinalScoreBindingGeneration == session.activity.bindingGeneration
        && session.activityFinalScoreAllocation == value.allocation
        && session.activityFinalScoreRevision == value.revision;
    const bool completeDelivered =
        session.activityMatchPublished
        && session.activityMatchBindingGeneration == session.activity.bindingGeneration
        && session.activityMatchAllocation == value.allocation
        && session.activityMatchRevision == value.revision
        && (session.activityMatchPhase == phases.completion
            || session.activityMatchPhase == phases.postgame
            || session.activityMatchPhase == phases.advance);
    const bool postgameDelivered =
        session.activityMatchPublished
        && session.activityMatchBindingGeneration == session.activity.bindingGeneration
        && session.activityMatchAllocation == value.allocation
        && session.activityMatchRevision == value.revision
        && (session.activityMatchPhase == phases.postgame
            || session.activityMatchPhase == phases.advance);
    {
        if (!value.scriptFlowReady) {
            return phases.active;
        }
        using Phase = match::script_policy::Phase;
        const auto desired = value.scriptFlow.phase;
        if (!value.finished) {
            return desired == Phase::intro ? phases.intro : phases.active;
        }
        // These are delivery barriers, not a second mode policy. Never skip a native consumer.
        if (!finalDelivered) {
            return phases.active;
        }
        if (desired == Phase::complete || !completeDelivered) {
            return phases.completion;
        }
        if (desired == Phase::postgame || !postgameDelivered) {
            return phases.postgame;
        }
        return phases.advance;
    }
}

/**
 * Simulation frames a scoreboard change made only by the installed time recurrences waits before
 * it is published. Native FDAF80 shows a time column as frames over 4E4CA0's simulation frames
 * per second, and every applied scoreboard moves the stamp FD9B10 compares, which wipes and
 * rebuilds the whole HUD panel (C78B50). One displayed second is the finest step the reader shows.
 */
inline constexpr std::uint64_t kTimedRefreshFrames =
    state::activity::activity_clock::kSimulationFramesPerSecond;

/**
 * Whether the scoreboard moved in a way this recipient must receive now. Any change other than
 * the per-frame time recurrences is due at once; a time-only change is due once a displayed
 * second of frames has elapsed since the delivered body, and always once the match has finished,
 * whose final revision the completion phases key to.
 */
[[nodiscard]] inline bool
match_revision_due(const Session& session,
                   const state::activity::match_state::Publication& value) noexcept {
    if (session.activityMatchRevision == value.revision) {
        return false;
    }
    const std::uint32_t deliveredEvents =
        session.activityMatchRevision - session.activityMatchTimedRevisions;
    if (deliveredEvents != value.revision - value.timedRevisions || value.finished) {
        return true;
    }
    return value.statisticFrames < session.activityMatchStatisticFrames
           || value.statisticFrames - session.activityMatchStatisticFrames >= kTimedRefreshFrames;
}

/**
 * Whether the HUD shows the match clock: the match runs a clock and publishes its active phase,
 * the only lifetime for which native 1567000 hands the timer seconds to its widget.
 */
[[nodiscard]] constexpr bool
match_clock_visible(bool timed,
                    std::uint8_t lifetime,
                    const state::activity::match_modes::Definition& mode) noexcept {
    return timed && lifetime == mode.phases.active;
}

/**
 * Whether the hud_sensor lists the mode's capture zones: only the Control objective HUD (0xBEA020
 * mode 1) walks that list, through its record store 0xBEAF30.
 */
[[nodiscard]] constexpr bool
lists_hud_zones(const state::activity::match_modes::Definition& mode) noexcept {
    return mode.objectiveHud == state::activity::match_modes::objective_hud::kControl
           && mode.zones.count != 0;
}

[[nodiscard]] inline bool
match_publication_due(const Session& session,
                      const state::activity::match_state::Publication& value) noexcept {
    const auto phase = match_publication_phase(session, value);
    if (value.mode == nullptr) {
        return false;
    }
    if (value.aborted) {
        return !session.activityMatchAborted || !session.activityMatchPublished
               || session.activityMatchBindingGeneration != session.activity.bindingGeneration
               || session.activityMatchAllocation != value.allocation;
    }
    if (!value.scriptFlowReady) {
        return false;
    }
    return !session.activityMatchPublished || session.activityMatchAllocation != value.allocation
           || session.activityMatchBindingGeneration != session.activity.bindingGeneration
           || match_revision_due(session, value) || session.activityMatchPhase != phase
           || (value.finished && phase == value.mode->phases.active);
}

/** Close only the activity link whose failed-generation roster has reached this recipient. */
[[nodiscard]] inline bool match_abort_delivered(const Session& session,
                                                std::uint64_t allocation) noexcept {
    return session.activity.role != ActivityClientRole::none && session.activityMatchPublished
           && session.activityMatchAborted
           && session.activityMatchBindingGeneration == session.activity.bindingGeneration
           && session.activityMatchAllocation == allocation;
}

} // namespace sunrise::server::bap::encrypted::push::activity

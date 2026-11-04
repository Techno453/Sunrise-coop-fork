#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "../../../middleware/bap/activity_message/cinematic_record.h"
#include "../../../middleware/bap/activity_message/sensor_sense_update.h"
#include "../../../middleware/content/packages/tables/statistic_program.h"
#include "../activity_clock/definition.h"
#include "../activity_timer/definition.h"
#include "../match_modes/definition.h"
#include "script_policy.h"

namespace sunrise::state::activity::match_modes {
struct Definition;
}
namespace sunrise::state::activity::match_state {

/** Native activity_lifetime of active play; the spawn gate passes it. */
inline constexpr std::uint8_t kActive = 3;
/** Native activity_intro selection accepts lifetime 1 or 2; the match intro uses 2. */
inline constexpr std::uint8_t kIntro = 2;
/** Native activity_lifetime that shows the match result. */
inline constexpr std::uint8_t kComplete = 6;
/** Native activity_lifetime=postgame, which native FE2FA0 emits as state 7. */
inline constexpr std::uint8_t kPostgame = 7;
/** Native activity_lifetime on which D4B870 leaves the activity. */
inline constexpr std::uint8_t kAdvance = 8;
enum class EndReason : std::uint8_t { none, timeLimit, scoreLimit };

/** Players one match tracks, one per participation seat. */
inline constexpr std::size_t kPlayerCapacity = script_policy::kResultCapacity;
/**
 * Shortest interval between two time-driven script projections of one match. A recipient's match
 * roster follows at this cadence, so a projection is never older than the roster that carries it.
 */
inline constexpr std::uint64_t kProjectionIntervalMilliseconds = 16;
/** Scoreboard inputs whose placements one match keeps: one ordering per participation seat. */
inline constexpr std::size_t kResultProjectionCapacity = kPlayerCapacity;
/** Named participation statistics native BF6380 reports for each player. */
inline constexpr std::size_t kNamedStatisticCount = 2;
/** FNV-1 of `damage_dealt`, the first named participation float BF6380 accumulates. */
inline constexpr std::uint32_t kDamageDealtStatistic = 0x5981F119U;
/**
 * The second named participation float BF6380 accumulates; installed statistic programs add it
 * to `damage_dealt`. Its spelling is not known.
 */
inline constexpr std::uint32_t kSecondNamedStatistic = 0x165CB1E2U;

/** Allocation-qualified state identity; publication cursors are recipient-owned. */
struct Publication final {
    std::uint64_t allocation{};
    std::uint32_t revision{};
    bool finished{};
    bool advanceReady{};
    bool postgameReady{};
    const match_modes::Definition* mode{};
    bool introActive{};
    /** State::timedRevisions at this revision. */
    std::uint32_t timedRevisions{};
    /** Simulation frames the retained statistics have advanced through. */
    std::uint64_t statisticFrames{};
    /** Script's desired lifecycle; recipient acknowledgement barriers are applied separately. */
    script_policy::FlowDecision scriptFlow{};
    bool scriptFlowReady{};
    bool aborted{};
};

/** Actor-scoped sequence window; replay protection survives publication and peer fanout. */
struct Sequence final {
    std::uint64_t sourcePlayer{};
    std::uint64_t seen{};
    std::uint32_t highest{};
    bool initialized{};
};

struct Score final {
    std::uint64_t playerKey{};
    std::int32_t value{};
};

/**
 * Retained total of one native partition. It is credited inside the accepted score transaction,
 * so a member's departure or partition change never removes points the team already earned.
 */
struct TeamScore final {
    std::int8_t partition{-1};
    std::int32_t value{};
};

/** One row per participation seat: sixteen seats can hold at most sixteen partitions. */
inline constexpr std::size_t kTeamScoreCapacity = kPlayerCapacity;

/**
 * Partition holding one capture zone, named by the native zone handle the simulating peer reports
 * (type-6 `+0x328`). Retained with the match like the team totals.
 */
struct ZoneHold final {
    std::uint32_t zone{};
    /** -1 while nobody holds the zone. */
    std::int8_t owner{-1};
    /** The mode's zone placement this handle was captured as, or -1 before a named capture. */
    std::int8_t placement{-1};
    /** Owner changes accepted for this zone; the zone object's Auth carries it as its revision. */
    std::int32_t ownerRevision{};
};

/**
 * Zone rows kept per allocation. Control authors three zones; the spare rows absorb a zone a new
 * simulating peer reports under another handle without displacing a held one.
 */
inline constexpr std::size_t kZoneCapacity = 8;

/** One player's scoreboard statistic values, in config column order. */
struct PlayerStatistics final {
    std::uint64_t playerKey{};
    std::array<float,
               middleware::content::packages::tables::statistic_program::kPlayerStatisticCapacity>
        values{};
};

struct Notice final {
    std::uint64_t player{};
    std::uint64_t joinIdentity{};
    std::uint32_t sequence{};
    std::int32_t amount{};
    std::uint16_t target{};
};

/** Notices one recipient batch carries. */
inline constexpr std::size_t kNoticeBatchCapacity = 8;
/** Notices one match retains; the oldest is dropped when it is full. */
inline constexpr std::size_t kNoticeCapacity = 64;

/** One recipient batch; commit only after its complete transport frame is published. */
struct NoticeBatch final {
    std::uint64_t allocation{};
    std::uint64_t memberKey{};
    std::array<Notice, kNoticeBatchCapacity> notices{};
    std::uint8_t count{};
};

struct IntroMember final {
    std::uint64_t memberKey{};
    std::uint64_t joinIdentity{};
    std::uint32_t group{};
    std::uint16_t slot{};
    Sequence reports{};
    /** Native damage observed while loading is excluded from this match's input columns. */
    std::array<float, kNamedStatisticCount> namedBaseline{};
    std::uint64_t cinematicStartedAtMilliseconds{};
    bool ready{};
    bool started{};
    bool finished{};
    bool failed{};
};

/** The script's placements for one scoreboard input, kept so a repeat does not enter Lua. */
struct ResultProjection final {
    script_policy::ResultsInput input{};
    script_policy::ResultsDecision decision{};
};

/** @return True when two scoreboard inputs would ask the script the same question. */
[[nodiscard]] inline bool same_results(const script_policy::ResultsInput& left,
                                       const script_policy::ResultsInput& right) noexcept {
    if (left.count != right.count || left.titlesAvailable != right.titlesAvailable
        || left.count > left.rows.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.count; ++i) {
        if (std::bit_cast<std::uint32_t>(left.rows[i].points)
                != std::bit_cast<std::uint32_t>(right.rows[i].points)
            || left.rows[i].live != right.rows[i].live) {
            return false;
        }
    }
    return true;
}

/** Activity authority result, retained until the containing activity allocation is retired. */
struct State final {
    EndReason endReason{};
    std::uint64_t endedAtTicks{};
    std::array<Sequence, kPlayerCapacity> sequences{};
    std::array<Score, kPlayerCapacity> scores{};
    /** Filled only for a mode whose definition scores teams; one row per scoring partition. */
    std::array<TeamScore, kTeamScoreCapacity> teamScores{};
    /** Filled only for a mode with capture zones, from the accepted type-6 zone reports. */
    std::array<ZoneHold, kZoneCapacity> zones{};
    std::array<Sequence, kPlayerCapacity> statisticSequences{};
    std::array<PlayerStatistics, kPlayerCapacity> statistics{};
    std::uint64_t statisticFrames{};
    std::uint32_t revision{};
    /**
     * Revision moves made only by the installed time recurrences (`advance_statistics`), which
     * run every simulation frame; `revision - timedRevisions` moves with every other change.
     */
    std::uint32_t timedRevisions{};
    /** Filled only on snapshot, identifies the containing activity allocation for recipients. */
    std::uint64_t allocationRevision{};
    /** The script's completion/results projection; see `scriptFlow`. */
    bool advanceReady{};
    bool postgameReady{};
    std::array<Notice, kNoticeCapacity> notices{};
    std::uint32_t droppedNotices{};
    std::uint8_t noticeCount{};
    std::array<IntroMember, kPlayerCapacity> introMembers{};
    std::array<middleware::bap::activity_message::cinematic_record::Command,
               middleware::bap::activity_message::cinematic_record::kCommandCapacity>
        cinematics{};
    /** Zero waits for readiness; subsequent steps retain played clips; hasStarted owns play. */
    std::uint8_t introStep{};
    /** Reject reusing an already-issued native cinematic generation in a scripted sequence. */
    std::uint32_t scriptedIntroUsed{};
    bool hasStarted{};
    std::uint64_t startedAtTicks{};
    /**
     * The script's lifecycle projection, never a second retained phase controller. Readers copy
     * it without entering Lua: a transaction that admits a decided event projects again before
     * it releases State, and the time-driven refresh projects at most once per
     * `kProjectionIntervalMilliseconds`. It is stamped with the revision and clock tick it was
     * made at.
     */
    script_policy::FlowDecision scriptFlow{};
    bool scriptFlowReady{};
    bool aborted{};
    /** False once the projection misses a change the revision does not count (a time expiry). */
    bool projected{};
    std::uint32_t projectedRevision{};
    std::uint64_t projectedTicks{};
    std::uint64_t projectedAllocation{};
    /** The mode declaration the script made for `definitionAllocation`, asked once. */
    script_policy::ModeDefinition definition{};
    std::uint64_t definitionAllocation{};
    /** Clock milliseconds of the last time-driven refresh. */
    std::uint64_t refreshedAtMilliseconds{};
    bool refreshed{};
    /** Placements for the scoreboard inputs of `resultsAllocation`, replaced oldest first. */
    std::array<ResultProjection, kResultProjectionCapacity> results{};
    std::uint64_t resultsAllocation{};
    std::uint8_t resultCount{};
    std::uint8_t nextResult{};
};

/**
 * Statistics are no lifecycle input, so a move made by them alone keeps a projection that was
 * current before it current.
 * @param state Match whose revision just moved.
 * @param before Revision before the move.
 */
inline void carry_projection(State& state, std::uint32_t before) noexcept {
    if (state.projectedRevision == before) {
        state.projectedRevision = state.revision;
    }
}

/** @return How many zones `partition` holds. */
[[nodiscard]] constexpr std::uint8_t zones_held(const State& state,
                                                std::int8_t partition) noexcept {
    std::uint8_t held = 0;
    for (const auto& zone : state.zones) {
        if (partition >= 0 && zone.zone != 0 && zone.owner == partition) {
            ++held;
        }
    }
    return held;
}

/** Readiness starts the intro; native landing completion releases active play. */
[[nodiscard]] constexpr bool intro_active(const State& state) noexcept {
    return state.introStep != 0 && !state.hasStarted && state.endReason == EndReason::none;
}

/** The match timer shares world time but stays paused until native landing completes. */
inline void match_timer(const State& state, activity_timer::ResolvedTimer& timer) noexcept {
    timer.epoch = state.startedAtTicks;
    timer.currentAtEpoch = 0;
    timer.remaining = timer.maximum;
    timer.rate = state.hasStarted ? 1.0F : 0.0F;
}

/** Keep one shared completion epoch; publishing again never restarts either phase timer. */
inline void completion_timer(const State& state,
                             std::uint8_t phase,
                             std::int32_t completeSeconds,
                             std::int32_t postgameSeconds,
                             activity_timer::ResolvedTimer& timer) noexcept {
    if (state.endReason == EndReason::none || completeSeconds <= 0 || postgameSeconds < 0) {
        return;
    }
    timer.epoch = state.endedAtTicks;
    if (phase == kComplete || phase == kPostgame || phase == kAdvance) {
        const bool postgame = phase != kComplete;
        if (postgame) {
            timer.epoch +=
                static_cast<std::uint64_t>(completeSeconds) * activity_clock::kTicksPerSecond;
        }
        timer.maximum = static_cast<std::uint64_t>(postgame ? postgameSeconds : completeSeconds)
                        * activity_clock::kTicksPerSecond;
        timer.remaining = timer.maximum;
        timer.currentAtEpoch = 0;
        timer.rate = 1.0F;
    } else {
        const auto elapsed = state.endedAtTicks >= state.startedAtTicks
                                 ? state.endedAtTicks - state.startedAtTicks
                                 : 0;
        timer.currentAtEpoch = elapsed < timer.maximum ? elapsed : timer.maximum;
        timer.remaining = timer.maximum - timer.currentAtEpoch;
        timer.rate = 0.0F;
    }
}

/** The replay window remembers the sequences up to 63 below the highest one. */
inline constexpr std::uint32_t kSequenceWindow = 64;
/** A modular distance below half the 32-bit range moves the window forward. */
inline constexpr std::uint32_t kForwardDistanceLimit = 0x80000000U;

/** Bounded actor-sequence replay window. Modular distance handles the wrapping native counter. */
[[nodiscard]] constexpr bool accept_sequence(Sequence& state, std::uint32_t sequence) noexcept {
    if (!state.initialized) {
        if (sequence == 0) {
            return false;
        }
        state.initialized = true;
        state.highest = sequence;
        state.seen = 1;
        return true;
    }
    const std::uint32_t distance = sequence - state.highest;
    if (distance != 0 && distance < kForwardDistanceLimit) {
        state.seen = distance >= kSequenceWindow ? 1 : (state.seen << distance) | 1;
        state.highest = sequence;
        return true;
    }
    const std::uint32_t age = state.highest - sequence;
    if (age >= kSequenceWindow || (state.seen & (std::uint64_t{1} << age)) != 0) {
        return false;
    }
    state.seen |= std::uint64_t{1} << age;
    return true;
}

} // namespace sunrise::state::activity::match_state

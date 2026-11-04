#pragma once

#include <cstdint>
#include <span>

#include "../../../middleware/bap/activity_message/kill_incident_codec.h"
#include "../../../middleware/bap/activity_message/sensor_sense_update.h"
#include "../../build_data/activity_rules/runtime.h"
#include "definition.h"

namespace sunrise::middleware::bap::activity_message::scoreboard_record {
struct Record;
}

namespace sunrise::state::activity {
struct SessionRecord;
namespace destination {
struct DestinationSelection;
}
} // namespace sunrise::state::activity

namespace sunrise::state::activity::match_state {

/** Caller holds State's exclusive lock. A fault permanently aborts this allocation. */
[[nodiscard]] bool evaluate_script(SessionRecord& record,
                                   std::size_t slot,
                                   const match_modes::Definition& mode,
                                   const script_policy::Request& request,
                                   script_policy::Response& response) noexcept;
void abort_script(SessionRecord& record,
                  std::size_t slot,
                  std::uint32_t modeKey,
                  std::string_view reason = "native_contract") noexcept;
/** Reads the failed allocation without running Lua. */
[[nodiscard]] bool aborted(std::uint64_t sessionId, std::uint64_t& allocation) noexcept;

/**
 * Caller holds State's exclusive lock. Projects the script's lifecycle and mode declaration into
 * the match state when the cached projection is not current for its revision and allocation.
 * @param nowTicks Clock tick the projection is made at.
 */
void project_lifecycle(SessionRecord& record,
                       std::size_t slot,
                       const build_data::activity_rules::Rules& rules,
                       std::uint64_t nowTicks) noexcept;

/** One intro step taken under State's lock, logged once the lock is released. */
struct IntroReport final {
    bool taken{};
    bool failure{};
    std::uint8_t step{};
    bool started{};
    std::uint64_t startedAtTicks{};
};
/**
 * Caller holds State's exclusive lock. Asks the script for the next intro step and takes it.
 * @param nowTicks Clock tick a started match starts at.
 * @return The step taken, for `report_intro` after the lock is released.
 */
[[nodiscard]] IntroReport advance_intro(SessionRecord& record,
                                        std::size_t slot,
                                        const build_data::activity_rules::Rules& rules,
                                        std::uint64_t nowTicks) noexcept;
/** Logs an intro step `advance_intro` took; call without State's lock. */
void report_intro(std::uint64_t sessionId, const IntroReport& report) noexcept;

/**
 * The activity whose installed mode governs one allocation: the activity the client asked from,
 * which names the selected private-match mode, or the host's own destination row when a forced
 * destination left no request. The two agree for every concrete map activity.
 */
[[nodiscard]] std::int16_t
match_activity(const destination::DestinationSelection& destination) noexcept;

[[nodiscard]] bool
assignment(std::uint64_t sessionId, std::uint64_t player, std::int8_t& partition) noexcept;

/** Reads the captured Lua mode declaration for this exact allocation and validates native
 * compatibility. */
[[nodiscard]] bool script_definition(std::uint64_t sessionId,
                                     std::uint64_t allocationRevision,
                                     script_policy::ModeDefinition& output) noexcept;

/** Bind a recipient's actual published local seat and verified common cinematic group. */
void bind_intro_member(std::uint64_t sessionId,
                       std::uint64_t memberKey,
                       std::uint32_t group,
                       std::uint16_t slot) noexcept;
void observe_participation(
    std::uint64_t sessionId,
    std::uint64_t memberKey,
    const middleware::bap::activity_message::sensor_sense_update::Update& update,
    std::uint64_t nowMilliseconds) noexcept;
/** Applies one member's named participation statistics; reads the match without entering Lua. */
void observe_named_statistics(
    std::uint64_t sessionId,
    std::uint64_t memberKey,
    const middleware::bap::activity_message::sensor_sense_update::Update& update) noexcept;
void observe_cinematic(std::uint64_t sessionId,
                       std::uint64_t memberKey,
                       std::span<const std::byte> canonical,
                       std::uint64_t nowMilliseconds) noexcept;
/** Caller holds the activity lock; native expressions run once per elapsed active frame. */
void advance_statistics(
    SessionRecord& record,
    const middleware::content::packages::tables::statistic_program::Catalog& catalog,
    std::uint64_t nowTicks) noexcept;

struct MemberProjection final {
    std::int8_t partition{-1};
    bool assigned{};
};
/** One transaction for all roster seats; copies no partial result on a policy fault or stale
 * allocation. */
[[nodiscard]] bool project_members(std::uint64_t sessionId,
                                   std::uint64_t allocation,
                                   std::span<const std::uint64_t> players,
                                   std::span<MemberProjection> output) noexcept;

/**
 * Activity-host boundary for a decoded incident from a currently bound member. `classification`
 * holds the keys of the incident targets the reporting client selected for the kill.
 */
void observe_kill(std::uint64_t sessionId,
                  std::uint64_t senderMemberKey,
                  const middleware::bap::activity_message::incident::KillFacts& facts,
                  std::uint64_t nowMilliseconds,
                  std::span<const std::uint32_t> classification = {}) noexcept;

/**
 * Zone transaction for one decoded type-6 report from a currently bound member: records the
 * reported zone's new owner, which the simulating peer already applied, and, for a report whose
 * participants are members of one partition, credits the mode's zone award to that team once and
 * to each participating member, with the mode's zone notice. A change the authority refuses (a
 * inactive match or replay) republishes its own owner under a newer revision.
 * Shares the kill replay window, since both come from one source sequence. `classification` holds
 * the keys of the incident targets the simulating peer selected for the report.
 */
void observe_capture(std::uint64_t sessionId,
                     std::uint64_t senderMemberKey,
                     const middleware::bap::activity_message::incident::CaptureFacts& facts,
                     std::uint64_t nowMilliseconds,
                     std::span<const std::uint32_t> classification = {}) noexcept;

/** Applies client-completed events through installed programs before the scoring transaction. */
void observe_incident(std::uint64_t sessionId,
                      std::uint64_t senderMemberKey,
                      std::span<const std::byte> canonical,
                      std::uint64_t nowMilliseconds) noexcept;

/**
 * Returns one immutable retained result snapshot only for an installed, supported activity mode.
 * Copies the cached script projection; Lua runs only when the projection is not current, which
 * is before the allocation's first projection.
 */
[[nodiscard]] bool snapshot(std::uint64_t sessionId,
                            State& output,
                            build_data::activity_rules::Rules* settings = nullptr) noexcept;

/**
 * Reads the retained match under State's shared lock, as it stands, without entering Lua. For
 * probes that ask whether a match exists, is finished or aborted, and for its rules.
 */
[[nodiscard]] bool read(std::uint64_t sessionId,
                        State& output,
                        build_data::activity_rules::Rules* settings = nullptr) noexcept;

/**
 * Projects Lua placements for this exact allocation; failure leaves every title unchanged. The
 * script is asked once per distinct scoreboard input; a repeat copies the cached placements.
 */
[[nodiscard]] bool project_script_results(
    std::uint64_t sessionId,
    std::uint64_t allocationRevision,
    middleware::bap::activity_message::scoreboard_record::Record& output) noexcept;

/**
 * Advances only an initialized match clock and returns a small publication identity. The
 * time-driven projection (intro, expiry, time recurrences, lifecycle) runs at most once per
 * `kProjectionIntervalMilliseconds`; a call inside that interval copies the cached projection.
 */
[[nodiscard]] bool
publication(std::uint64_t sessionId, std::uint64_t nowMilliseconds, Publication& output) noexcept;

[[nodiscard]] bool
pending_notices(std::uint64_t sessionId, std::uint64_t memberKey, NoticeBatch& output) noexcept;
/** Call under the serialized BAP publication transaction; amount is the published prefix. */
void commit_notices(std::uint64_t sessionId, const NoticeBatch& batch, std::size_t amount) noexcept;

} // namespace sunrise::state::activity::match_state

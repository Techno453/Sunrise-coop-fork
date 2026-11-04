#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sunrise::state::activity::match_state::script_policy {

/** Bounded native incident target keys. Lua prices existing classifications, never makes them. */
inline constexpr std::size_t kClassificationCapacity = 64;
struct Classification final {
    std::array<std::uint32_t, kClassificationCapacity> keys{};
    std::size_t count{};
};

/** Rule inputs after native kill qualification; no combat or identity writes are exposed. */
struct Input final {
    std::int32_t finalBlowScore{};
    std::int32_t score{};
    std::int32_t scoreLimit{};
    std::int32_t highestScore{};
    std::int32_t killScore{};
    std::int32_t teamScore{};
    std::int32_t highestTeamScore{};
    std::int32_t advantageKillBonus{};
    std::int32_t powerPlayKillBonus{};
    std::int8_t killerPartition{-1};
    std::int8_t victimPartition{-1};
    std::uint8_t killerZones{};
    std::uint8_t zoneCount{};
    bool teamScoring{};
    Classification classification{};
};

/** Symbolic notices resolved only through the selected mode's installed type-25 bindings. */
enum class Notice : std::uint8_t {
    installed,
    none,
    killScore,
    advantageKill,
    powerPlayKill,
    zoneA,
    zoneB,
    zoneC,
    count
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Notice::count)> kNoticeNames{
    "installed",
    "none",
    "kill_score",
    "advantage_kill",
    "power_play_kill",
    "zone_a",
    "zone_b",
    "zone_c"};
enum class Credit : std::uint8_t { installed, player, team, both, count };
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Credit::count)> kCreditNames{
    "installed", "player", "team", "both"};
[[nodiscard]] constexpr bool credits_player(Credit credit) noexcept {
    return credit == Credit::player || credit == Credit::both;
}
[[nodiscard]] constexpr bool credits_team(Credit credit) noexcept {
    return credit == Credit::team || credit == Credit::both;
}
/** A candidate only. The match transaction validates and commits all fields together. */
struct Decision final {
    std::int32_t points{};
    bool complete{};
    Notice notice{};
    Credit credit{};
};

/** Explicit declarations of supported native behavior. */
enum class FactionPolicy : std::uint8_t { unspecified, freeForAll, partitions };
enum class RespawnPolicy : std::uint8_t { unspecified, installed };
enum class LifePolicy : std::uint8_t { unspecified, unlimited };
enum class SpawnPolicy : std::uint8_t { unspecified, map };
enum class RoundPolicy : std::uint8_t { unspecified, single };
enum class Objective : std::uint8_t { none, controlZones };
[[nodiscard]] constexpr bool has_capture_zones(Objective objective) noexcept {
    return objective == Objective::controlZones;
}
enum class Hud : std::uint8_t { none, control };
/** Native scoreboard team-list capacity. */
inline constexpr std::uint8_t kTeamCapacity = 12;
struct ModeDefinition final {
    FactionPolicy factions{};
    RespawnPolicy respawn{};
    LifePolicy lives{};
    SpawnPolicy spawns{};
    RoundPolicy rounds{};
    std::uint8_t teamCount{};
    bool teamScoring{};
    Objective objectives{};
    Hud hud{};
};
[[nodiscard]] constexpr bool supported(const ModeDefinition& mode) noexcept {
    const bool ffa = mode.factions == FactionPolicy::freeForAll && mode.teamCount == 0
                     && !mode.teamScoring && mode.objectives == Objective::none
                     && mode.hud == Hud::none;
    const bool teams =
        mode.factions == FactionPolicy::partitions && (mode.teamCount == 0 || mode.teamCount >= 2)
        && mode.teamCount <= kTeamCapacity
        && (mode.objectives == Objective::none || mode.objectives == Objective::controlZones)
        && (mode.hud == Hud::none
            || (mode.hud == Hud::control && mode.objectives == Objective::controlZones));
    return (ffa || teams) && mode.lives == LifePolicy::unlimited
           && mode.rounds == RoundPolicy::single && mode.respawn == RespawnPolicy::installed
           && mode.spawns == SpawnPolicy::map;
}

enum class Kind : std::uint8_t { kill, intro, time, flow, results, capture, definition, count };
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Kind::definition)> kHandlers{
    "on_match_kill",
    "on_match_intro",
    "on_match_time",
    "on_match_flow",
    "on_match_results",
    "on_match_capture"};
[[nodiscard]] constexpr bool required(Kind kind, const ModeDefinition& mode) noexcept {
    if (kind == Kind::capture) {
        return has_capture_zones(mode.objectives);
    }
    return true;
}
enum class Phase : std::uint8_t { active, intro, complete, postgame, advance, count };
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Phase::count)> kPhaseNames{
    "active", "intro", "complete", "postgame", "advance"};
inline constexpr std::array<std::string_view, 9> kModeFields{"factions",
                                                             "respawn",
                                                             "lives",
                                                             "spawns",
                                                             "rounds",
                                                             "team_count",
                                                             "team_scoring",
                                                             "objectives",
                                                             "hud"};
/** Maximum participation seats and result rows in one match. */
inline constexpr std::size_t kResultCapacity = 16;

/** Supported content bindings, not a playback order. Zero requests no new clip. */
enum class IntroClip : std::uint8_t {
    none,
    openingA,
    openingB,
    openingC,
    matchTitle,
    ffaLanding,
    count
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(IntroClip::count)>
    kIntroClipNames{"", "opening_a", "opening_b", "opening_c", "match_title", "ffa_landing"};

/** Readiness and playback reports have already passed identity and replay validation. */
struct IntroInput final {
    std::uint8_t step{};
    std::uint8_t capacity{};
    bool started{};
    bool finished{};
    bool allReady{};
    bool clipFinished{};
    std::uint32_t usedClips{};
};
struct IntroDecision final {
    std::uint8_t nextStep{};
    bool start{};
    IntroClip clip{};
};

/** Relative ticks avoid exposing identities or unsigned absolute clock values to Lua. */
struct TimeInput final {
    std::uint64_t elapsed{};
    std::uint64_t limit{};
    bool started{};
    bool finished{};
};
struct TimeDecision final {
    bool finish{};
    std::uint64_t elapsedAtEnd{};
};

struct FlowInput final {
    std::uint64_t elapsed{};
    std::uint64_t ticksPerSecond{};
    std::int32_t completeSeconds{};
    std::int32_t postgameSeconds{};
    bool started{};
    bool finished{};
    bool introActive{};
};
/** Desired phase and intervals; delivery barriers and native timer encoding stay in C++. */
struct FlowDecision final {
    Phase phase{};
    std::int32_t completeSeconds{};
    std::int32_t postgameSeconds{};
};

struct ResultRow final {
    float points{};
    bool live{};
};
struct ResultsInput final {
    std::array<ResultRow, kResultCapacity> rows{};
    std::size_t count{};
    bool titlesAvailable{true};
};
struct Placement final {
    std::uint8_t place{}; // One-based; zero means no title.
    bool tied{};
};
struct ResultsDecision final {
    std::array<Placement, kResultCapacity> rows{};
};

/** Validated capture facts. Participants are deduplicated and their maximum score is bounded. */
struct CaptureInput final {
    std::int32_t captureScore{};
    std::int32_t neutralizeScore{};
    std::int32_t maximumPlayerScore{};
    std::int32_t teamScore{};
    std::int32_t highestTeamScore{};
    std::int32_t scoreLimit{};
    std::int8_t partition{-1};
    std::uint8_t zone{}; // One-based authored placement, or zero when unclassified.
    std::uint8_t participantCount{};
    bool neutralized{};
    bool teamScoring{};
};

/** One typed callback request. Only the member selected by kind is read. */
struct Request final {
    Kind kind{};
    Input kill{};
    CaptureInput capture{};
    IntroInput intro{};
    TimeInput time{};
    FlowInput flow{};
    ResultsInput results{};
};
struct Response final {
    Decision kill{};
    Decision capture{};
    IntroDecision intro{};
    TimeDecision time{};
    FlowDecision flow{};
    ResultsDecision results{};
    ModeDefinition definition{};
};

/** Exact occupied record slot and generation; the evaluator must not acquire State's lock. */
struct Binding final {
    std::size_t slot{};
    std::uint64_t sessionId{};
    std::uint64_t createdRevision{};
    std::uint32_t modeKey{};
};

/** Diagnostics and retired heaps drain only after the outer State transaction unlocks. */
using Flusher = void (*)() noexcept;
using AbortReporter = void (*)(const Binding&, std::string_view) noexcept;
inline std::atomic<Flusher> g_flusher{};
inline std::atomic<AbortReporter> g_abortReporter{};
inline thread_local unsigned g_deferredDepth{};
inline void install_deferred(Flusher flush, AbortReporter report) noexcept {
    g_flusher.store(flush, std::memory_order_release);
    g_abortReporter.store(report, std::memory_order_release);
}
struct DeferredScope final {
    DeferredScope() noexcept {
        ++g_deferredDepth;
    }
    DeferredScope(const DeferredScope&) = delete;
    DeferredScope& operator=(const DeferredScope&) = delete;
    ~DeferredScope() noexcept {
        if (--g_deferredDepth == 0) {
            if (const auto flush = g_flusher.load(std::memory_order_acquire)) {
                flush();
            }
        }
    }
};
inline void report_abort(const Binding& binding, std::string_view reason) noexcept {
    if (const auto report = g_abortReporter.load(std::memory_order_acquire)) {
        report(binding, reason);
    }
}

using Evaluator = bool (*)(const Binding&, const Request&, Response&) noexcept;
inline std::atomic<Evaluator> g_evaluator{};

using Releaser = void (*)(std::size_t, std::uint64_t, std::uint64_t) noexcept;
inline std::atomic<Releaser> g_releaser{};
inline void install_releaser(Releaser release) noexcept {
    g_releaser.store(release, std::memory_order_release);
}
/** Called while State owns the allocation, before retirement or eviction. */
inline void release(std::size_t slot, std::uint64_t sessionId, std::uint64_t revision) noexcept {
    const auto releaser = g_releaser.load(std::memory_order_acquire);
    if (releaser != nullptr) {
        releaser(slot, sessionId, revision);
    }
}

/** Installed before ingress starts and removed after ingress stops. Missing means refusal. */
inline void install(Evaluator evaluator) noexcept {
    g_evaluator.store(evaluator, std::memory_order_release);
}

/** Does not fall back to a second C++ policy when the script is absent or fails. */
[[nodiscard]] inline bool
evaluate(const Binding& binding, const Request& input, Response& output) noexcept {
    const auto evaluator = g_evaluator.load(std::memory_order_acquire);
    Response candidate{};
    if (evaluator == nullptr || !evaluator(binding, input, candidate)) {
        return false;
    }
    output = candidate;
    return true;
}

} // namespace sunrise::state::activity::match_state::script_policy

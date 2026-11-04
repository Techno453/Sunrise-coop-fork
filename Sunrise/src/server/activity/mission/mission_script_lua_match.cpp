#include <algorithm>
#include <cmath>
#include <limits>

#include "../../../state/activity/match_modes/definition.h"
#include "mission_script_vm_internal.h"

namespace sunrise::server::activity::mission::lua_vm::detail {
namespace {
namespace policy = state::activity::match_state::script_policy;

lua_Integer integer(lua_State* state, int index, lua_Integer low, lua_Integer high) {
    if (!lua_isinteger(state, index)) {
        raise_lua_error(state, "match result must be an integer");
    }
    const auto value = lua_tointeger(state, index);
    if (value < low || value > high) {
        raise_lua_error(state, "match result outside allowed range");
    }
    return value;
}

bool boolean(lua_State* state, int index) {
    if (lua_type(state, index) != LUA_TBOOLEAN) {
        raise_lua_error(state, "match result must be boolean");
    }
    return lua_toboolean(state, index) != 0;
}

void ticks(lua_State* state, const char* key, std::uint64_t value) {
    if (value > static_cast<std::uint64_t>((std::numeric_limits<lua_Integer>::max)())) {
        raise_lua_error(state, "relative match time exceeds Lua integer range");
    }
    set_integer(state, key, static_cast<lua_Integer>(value));
}

void raw_field(lua_State* state, const char* key) {
    lua_pushstring(state, key);
    lua_rawget(state, -2);
}
void classification(lua_State* state, const policy::Classification& value) {
    if (value.count > value.keys.size()) {
        raise_lua_error(state, "too many native classifications");
    }
    lua_createtable(state, 0, static_cast<int>(value.count));
    for (std::size_t i = 0; i < value.count; ++i) {
        lua_pushboolean(state, 1);
        lua_rawseti(state, -2, static_cast<lua_Integer>(value.keys[i]));
    }
    lua_setfield(state, -2, "classification");
}

policy::Decision score_decision(lua_State* state,
                                std::int32_t playerScore,
                                std::int32_t teamScore,
                                bool teamScoring) {
    policy::Decision result{};
    result.complete = boolean(state, -3);
    if (!lua_isnil(state, -2)) {
        if (lua_type(state, -2) != LUA_TSTRING) {
            raise_lua_error(state, "score notice must be a name");
        }
        const auto& names = policy::kNoticeNames;
        const auto found = std::find(names.begin(), names.end(), lua_string_view(state, -2));
        if (found == names.end()) {
            raise_lua_error(state, "unknown score notice");
        }
        result.notice = static_cast<policy::Notice>(found - names.begin());
    }
    if (!lua_isnil(state, -1)) {
        if (lua_type(state, -1) != LUA_TSTRING) {
            raise_lua_error(state, "score credit must be a name");
        }
        const auto& names = policy::kCreditNames;
        const auto found = std::find(names.begin(), names.end(), lua_string_view(state, -1));
        if (found == names.end()) {
            raise_lua_error(state, "unknown score credit");
        }
        result.credit = static_cast<policy::Credit>(found - names.begin());
    }
    const auto credit =
        result.credit == policy::Credit::installed ? policy::Credit::player : result.credit;
    if (policy::credits_team(credit) && !teamScoring) {
        raise_lua_error(state, "team credit requires team scoring");
    }
    auto maximum = (std::numeric_limits<std::int32_t>::max)();
    if (policy::credits_player(credit)) {
        maximum = (std::min)(maximum, maximum - playerScore);
    }
    if (policy::credits_team(credit)) {
        maximum = (std::min)(maximum, (std::numeric_limits<std::int32_t>::max)() - teamScore);
    }
    result.points = static_cast<std::int32_t>(integer(state, -4, 0, maximum));
    return result;
}
} // namespace

void capture_match_definition(lua_State* state, Impl& impl) {
    lua_pushliteral(state, "mode");
    lua_rawget(state, 1);
    if (lua_type(state, -1) != LUA_TTABLE) {
        raise_lua_error(state, "match policy requires a mode definition");
    }
    const auto& fields = policy::kModeFields;
    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING
            || std::find(fields.begin(), fields.end(), lua_string_view(state, -2))
                   == fields.end()) {
            raise_lua_error(state, "unknown match mode field");
        }
        lua_pop(state, 1);
    }
    const auto require = [&](const char* field, std::string_view value) {
        raw_field(state, field);
        if (lua_type(state, -1) != LUA_TSTRING || lua_string_view(state, -1) != value) {
            raise_lua_error(
                state,
                lua_pushfstring(state, "unsupported or missing match mode field: %s", field));
        }
        lua_pop(state, 1);
    };
    // Each accepted value names an implemented native adapter.
    require("respawn", "installed");
    require("spawns", "map");
    policy::ModeDefinition candidate{policy::FactionPolicy::freeForAll,
                                     policy::RespawnPolicy::installed,
                                     policy::LifePolicy::unlimited,
                                     policy::SpawnPolicy::map,
                                     policy::RoundPolicy::single};
    require("rounds", "single");
    require("lives", "unlimited");
    raw_field(state, "factions");
    if (lua_type(state, -1) != LUA_TSTRING) {
        raise_lua_error(state, "missing factions policy");
    }
    const auto faction = lua_string_view(state, -1);
    if (faction == "partitions") {
        candidate.factions = policy::FactionPolicy::partitions;
    } else if (faction != "free_for_all") {
        raise_lua_error(state, "unsupported factions policy");
    }
    lua_pop(state, 1);
    raw_field(state, "team_count");
    if (candidate.factions == policy::FactionPolicy::partitions) {
        if (lua_type(state, -1) == LUA_TSTRING && lua_string_view(state, -1) == "members") {
            candidate.teamCount = 0; // Existing native member-derived team list.
        } else {
            candidate.teamCount =
                static_cast<std::uint8_t>(integer(state, -1, 2, policy::kTeamCapacity));
        }
    } else if (!lua_isnil(state, -1)) {
        candidate.teamCount = static_cast<std::uint8_t>(integer(state, -1, 0, 0));
    }
    lua_pop(state, 1);
    raw_field(state, "team_scoring");
    if (!lua_isnil(state, -1)) {
        candidate.teamScoring = boolean(state, -1);
    }
    lua_pop(state, 1);
    raw_field(state, "objectives");
    if (!lua_isnil(state, -1)) {
        if (lua_type(state, -1) != LUA_TSTRING) {
            raise_lua_error(state, "invalid objective declaration");
        }
        const auto name = lua_string_view(state, -1);
        if (name == "control_zones") {
            candidate.objectives = policy::Objective::controlZones;
        } else if (name != "none") {
            raise_lua_error(state, "unsupported objective declaration");
        }
    }
    lua_pop(state, 1);
    raw_field(state, "hud");
    if (!lua_isnil(state, -1)) {
        if (lua_type(state, -1) != LUA_TSTRING) {
            raise_lua_error(state, "invalid objective HUD");
        }
        const auto name = lua_string_view(state, -1);
        if (name == "control") {
            candidate.hud = policy::Hud::control;
        } else if (name != "none") {
            raise_lua_error(state, "unsupported objective HUD");
        }
    }
    lua_pop(state, 1);
    if (!policy::supported(candidate)) {
        raise_lua_error(state, "contradictory mode declaration");
    }
    impl.matchDefinition = candidate;
    lua_pop(state, 1);
}

int call_match_policy(lua_State* state) {
    auto& frame = active_frame(state);
    if (frame.matchRequest == nullptr) {
        raise_lua_error(state, "match request is missing");
    }
    const auto& request = *frame.matchRequest;
    auto& response = frame.matchResponse;
    lua_createtable(state, 0, 8);
    switch (request.kind) {
    case policy::Kind::kill: {
        const auto& value = request.kill;
        if (value.finalBlowScore < 0 || value.score < 0 || value.scoreLimit <= 0
            || value.highestScore < 0 || value.killScore < 0 || value.teamScore < 0
            || value.highestTeamScore < 0 || value.advantageKillBonus < 0
            || value.powerPlayKillBonus < 0) {
            raise_lua_error(state, "invalid match score facts");
        }
        set_integer(state, "final_blow_score", value.finalBlowScore);
        set_integer(state, "kill_score", value.killScore);
        set_integer(state, "score", value.score);
        set_integer(state, "score_limit", value.scoreLimit);
        set_integer(state, "highest_score", value.highestScore);
        set_integer(state, "team_score", value.teamScore);
        set_integer(state, "highest_team_score", value.highestTeamScore);
        set_integer(state, "killer_partition", value.killerPartition);
        set_integer(state, "victim_partition", value.victimPartition);
        set_integer(state, "killer_zones", value.killerZones);
        set_integer(state, "zone_count", value.zoneCount);
        set_integer(state, "advantage_kill_bonus", value.advantageKillBonus);
        set_integer(state, "power_play_kill_bonus", value.powerPlayKillBonus);
        set_boolean(state, "team_scoring", value.teamScoring);
        classification(state, value.classification);
        lua_call(state, 1, 4);
        response.kill = score_decision(state, value.score, value.teamScore, value.teamScoring);
        break;
    }
    case policy::Kind::capture: {
        const auto& value = request.capture;
        if (value.captureScore < 0 || value.neutralizeScore < 0 || value.maximumPlayerScore < 0
            || value.teamScore < 0 || value.highestTeamScore < 0
            || value.participantCount > policy::kResultCapacity
            || value.zone > state::activity::match_modes::kZonePlacementCapacity) {
            raise_lua_error(state, "invalid capture facts");
        }
        set_integer(state, "capture_score", value.captureScore);
        set_integer(state, "neutralize_score", value.neutralizeScore);
        set_integer(state, "maximum_player_score", value.maximumPlayerScore);
        set_integer(state, "team_score", value.teamScore);
        set_integer(state, "highest_team_score", value.highestTeamScore);
        set_integer(state, "score_limit", value.scoreLimit);
        set_integer(state, "partition", value.partition);
        set_integer(state, "zone", value.zone);
        set_integer(state, "participant_count", value.participantCount);
        set_boolean(state, "neutralized", value.neutralized);
        set_boolean(state, "team_scoring", value.teamScoring);
        lua_call(state, 1, 4);
        response.capture =
            score_decision(state, value.maximumPlayerScore, value.teamScore, value.teamScoring);
        if ((value.partition < 0 || value.participantCount == 0 || value.scoreLimit <= 0)
            && (response.capture.points != 0 || response.capture.complete)) {
            raise_lua_error(state, "capture has no eligible score recipients");
        }
        if (response.capture.points == 0 && response.capture.complete) {
            raise_lua_error(state, "capture completion requires a score award");
        }
        break;
    }
    case policy::Kind::intro: {
        const auto& value = request.intro;
        if (value.capacity == (std::numeric_limits<std::uint8_t>::max)()
            || value.step > value.capacity + 1) {
            raise_lua_error(state, "invalid intro bounds");
        }
        set_integer(state, "step", value.step);
        set_integer(state, "clip_capacity", value.capacity);
        set_boolean(state, "started", value.started);
        set_boolean(state, "finished", value.finished);
        set_boolean(state, "all_ready", value.allReady);
        set_boolean(state, "clip_finished", value.clipFinished);
        lua_call(state, 1, 3);
        response.intro = {
            static_cast<std::uint8_t>(integer(state, -3, value.step, value.capacity + 1)),
            boolean(state, -2)};
        if (!lua_isnil(state, -1)) {
            if (lua_type(state, -1) != LUA_TSTRING) {
                raise_lua_error(state, "intro clip must name a supported content binding");
            }
            const auto name = lua_string_view(state, -1);
            const auto found =
                std::find(policy::kIntroClipNames.begin() + 1, policy::kIntroClipNames.end(), name);
            if (found == policy::kIntroClipNames.end()) {
                raise_lua_error(state, "unknown intro clip binding");
            }
            response.intro.clip =
                static_cast<policy::IntroClip>(found - policy::kIntroClipNames.begin());
        }
        const bool advances = response.intro.nextStep != value.step;
        const bool plays = response.intro.clip != policy::IntroClip::none;
        if ((advances
             && (response.intro.nextStep != value.step + 1 || value.started || value.finished
                 || !(value.step == 0 ? value.allReady : value.clipFinished)))
            || (advances ? response.intro.start == plays : (response.intro.start || plays))
            || (plays
                && (response.intro.nextStep > value.capacity
                    || (value.usedClips & (1U << static_cast<unsigned>(response.intro.clip)))
                           != 0))) {
            raise_lua_error(state, "intro decision bypasses validated playback or readiness");
        }
        break;
    }
    case policy::Kind::time: {
        const auto& value = request.time;
        ticks(state, "elapsed_ticks", value.elapsed);
        ticks(state, "limit_ticks", value.limit);
        set_boolean(state, "started", value.started);
        set_boolean(state, "finished", value.finished);
        lua_call(state, 1, 2);
        response.time = {boolean(state, -2),
                         static_cast<std::uint64_t>(
                             integer(state, -1, 0, static_cast<lua_Integer>(value.elapsed)))};
        if (response.time.finish && (!value.started || value.finished)) {
            raise_lua_error(state, "time decision requires an active match");
        }
        break;
    }
    case policy::Kind::flow: {
        const auto& value = request.flow;
        ticks(state, "elapsed_ticks", value.elapsed);
        ticks(state, "ticks_per_second", value.ticksPerSecond);
        set_integer(state, "complete_seconds", value.completeSeconds);
        set_integer(state, "postgame_seconds", value.postgameSeconds);
        set_boolean(state, "started", value.started);
        set_boolean(state, "finished", value.finished);
        set_boolean(state, "intro_active", value.introActive);
        lua_call(state, 1, 3);
        if (lua_type(state, -3) != LUA_TSTRING) {
            raise_lua_error(state, "match phase must be a name");
        }
        const auto name = lua_string_view(state, -3);
        const auto& names = policy::kPhaseNames;
        const auto found = std::find(names.begin(), names.end(), name);
        if (found == names.end()) {
            raise_lua_error(state, "unknown match phase");
        }
        response.flow = {static_cast<policy::Phase>(found - names.begin()),
                         static_cast<std::int32_t>(
                             integer(state, -2, 1, (std::numeric_limits<std::int32_t>::max)())),
                         static_cast<std::int32_t>(
                             integer(state, -1, 0, (std::numeric_limits<std::int32_t>::max)()))};
        const bool terminal = response.flow.phase >= policy::Phase::complete
                              && response.flow.phase <= policy::Phase::advance;
        if ((terminal != value.finished)
            || (response.flow.phase == policy::Phase::intro)
                   != (!value.finished && value.introActive)) {
            raise_lua_error(state, "match phase contradicts retained lifecycle");
        }
        break;
    }
    case policy::Kind::results: {
        const auto& value = request.results;
        if (value.count > value.rows.size()) {
            raise_lua_error(state, "too many result rows");
        }
        set_boolean(state, "titles_available", value.titlesAvailable);
        lua_createtable(state, static_cast<int>(value.count), 0);
        for (std::size_t index = 0; index < value.count; ++index) {
            const auto& row = value.rows[index];
            if (!std::isfinite(row.points)) {
                raise_lua_error(state, "nonfinite match score");
            }
            lua_createtable(state, 0, 2);
            lua_pushnumber(state, row.points);
            lua_setfield(state, -2, "score");
            set_boolean(state, "live", row.live);
            lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
        }
        lua_setfield(state, -2, "players");
        lua_call(state, 1, 1);
        if (lua_type(state, -1) != LUA_TTABLE || lua_rawlen(state, -1) != value.count) {
            raise_lua_error(state, "placements must match supplied result rows");
        }
        for (std::size_t index = 0; index < value.count; ++index) {
            lua_rawgeti(state, -1, static_cast<lua_Integer>(index + 1));
            if (lua_type(state, -1) != LUA_TTABLE) {
                raise_lua_error(state, "invalid placement row");
            }
            raw_field(state, "place");
            const auto place = integer(state,
                                       -1,
                                       value.rows[index].live && value.titlesAvailable ? 1 : 0,
                                       value.rows[index].live && value.titlesAvailable
                                           ? static_cast<lua_Integer>(value.count)
                                           : 0);
            lua_pop(state, 1);
            raw_field(state, "tied");
            const bool tied = boolean(state, -1);
            if ((!value.rows[index].live || !value.titlesAvailable) && tied) {
                raise_lua_error(state, "inactive row cannot be tied");
            }
            response.results.rows[index] = {static_cast<std::uint8_t>(place), tied};
            lua_pop(state, 2);
        }
        break;
    }
    default:
        raise_lua_error(state, "unknown match request");
    }
    lua_settop(state, 0);
    lua_pushboolean(state, 1);
    return 1;
}
} // namespace sunrise::server::activity::mission::lua_vm::detail

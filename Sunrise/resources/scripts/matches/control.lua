-- Sunrise's reconstructed Control host policy, not recovered Bungie source.
-- Facts are copied from authoritative state. These bundled callbacks use no mutable Lua state.

-- Zones a team holds for the advantage and power play kill bonuses.
local ADVANTAGE_ZONES = 2
local POWER_PLAY_ZONES = 3
-- Capture notice for each zone, in zone order.
local ZONE_NOTICES = {"zone_a", "zone_b", "zone_c"}

--- @return True when the award brings the leading team score to the limit.
local function reaches_limit(context, points)
    local leading = math.max(context.highest_team_score, context.team_score + points)
    return leading >= context.score_limit
end

return {
    mode = {
        factions = "partitions",
        team_count = 2,
        team_scoring = true,
        objectives = "control_zones",
        hud = "control",
        respawn = "installed",
        lives = "unlimited",
        spawns = "map",
        rounds = "single",
    },

    on_match_kill = function(context)
        local points, notice = context.kill_score, "kill_score"
        if context.killer_zones >= POWER_PLAY_ZONES then
            points, notice = points + context.power_play_kill_bonus, "power_play_kill"
        elseif context.killer_zones >= ADVANTAGE_ZONES then
            points, notice = points + context.advantage_kill_bonus, "advantage_kill"
        end
        return points, reaches_limit(context, points), notice, "both"
    end,

    on_match_capture = function(context)
        if context.partition < 0 or context.participant_count == 0 or context.score_limit <= 0 then
            return 0, false, "none", "both"
        end
        local points = context.neutralized and context.neutralize_score or context.capture_score
        local notice = not context.neutralized and ZONE_NOTICES[context.zone] or "none"
        return points, points > 0 and reaches_limit(context, points), notice, "both"
    end,

    on_match_intro = common.intro,

    on_match_time = common.time,

    on_match_flow = common.flow,

    on_match_results = common.team_results,
}

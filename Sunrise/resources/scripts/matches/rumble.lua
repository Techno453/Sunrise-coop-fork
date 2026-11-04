-- Sunrise's reconstructed Rumble host policy, not recovered Bungie source.
-- Facts are copied from authoritative state. These bundled callbacks use no mutable Lua state.
return {
    mode = {
        factions = "free_for_all",
        respawn = "installed",
        lives = "unlimited",
        spawns = "map",
        rounds = "single",
    },

    on_match_kill = function(context)
        local points = context.final_blow_score
        local leading = math.max(context.highest_score, context.score + points)
        return points, leading >= context.score_limit
    end,

    on_match_intro = common.intro,

    on_match_time = common.time,

    on_match_flow = common.flow,

    -- Competition ranking: equal scores share a place, and inactive rows stay unplaced.
    on_match_results = function(context)
        local placements = {}
        for index, player in ipairs(context.players) do
            local higher, equal = 0, 0
            if player.live then
                for _, other in ipairs(context.players) do
                    if other.live then
                        if other.score > player.score then higher = higher + 1 end
                        if other.score == player.score then equal = equal + 1 end
                    end
                end
            end
            placements[index] = {place = player.live and higher + 1 or 0, tied = equal > 1}
        end
        return placements
    end,
}

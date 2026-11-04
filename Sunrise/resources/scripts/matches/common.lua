-- Shared reconstructed Sunrise match policy, not recovered Bungie source.
-- Private Rumble and Control use the fireteam host, like PvE. Unsupported modes only load
-- the map; public playlists require the separate matchmaking implementation.
-- Native code owns combat, capture progress, spawn execution and HUD rendering.
-- Loaded from the bundled resource into each isolated match VM before its mode source.
local common = {}

-- Opening clips in play order. The match starts once the last one has finished.
local INTRO_CLIPS = {"opening_a", "opening_b", "opening_c", "match_title", "ffa_landing"}

--- @return The next intro step, whether the match starts now, and the clip to play if any.
function common.intro(context)
    if context.started or context.finished then
        return context.step, false
    end
    local ready = context.step == 0 and context.all_ready
        or context.step > 0 and context.clip_finished
    if not ready then
        return context.step, false
    end
    local next_step = context.step + 1
    local clip = INTRO_CLIPS[next_step]
    return next_step, clip == nil, clip
end

--- @return Whether the time limit ends the match now, and the tick it ended on.
function common.time(context)
    if context.started and not context.finished and context.limit_ticks > 0
        and context.elapsed_ticks >= context.limit_ticks then
        return true, context.limit_ticks
    end
    return false, 0
end

--- @return The phase name, then the completion and postgame lengths in seconds.
function common.flow(context)
    local phase = "active"
    if context.finished then
        local ticks = context.ticks_per_second
        local completion = context.complete_seconds * ticks
        local advance = (context.complete_seconds + context.postgame_seconds) * ticks
        if context.elapsed_ticks >= advance then
            phase = "advance"
        elseif context.elapsed_ticks >= completion then
            phase = "postgame"
        else
            phase = "complete"
        end
    elseif context.intro_active then
        phase = "intro"
    end
    return phase, context.complete_seconds, context.postgame_seconds
end

--- @return One unplaced row per player: team modes install no individual placement titles.
function common.team_results(context)
    local placements = {}
    for index in ipairs(context.players) do
        placements[index] = {place = 0, tied = false}
    end
    return placements
end

return common

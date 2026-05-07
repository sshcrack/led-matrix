-- fire.lua
-- Classic "DOOM-style" fire simulation on the LED matrix.
-- A heat buffer propagates upward each frame, cooling as it rises.
-- Drop into lua_scenes/ and restart the server.

name = "lua_fire"

-- ─── Setup ────────────────────────────────────────────────────────────────────

function setup()
    define_property("intensity",  "float", 0.92, 0.5,  1.0)
    define_property("cooling",    "float", 0.08, 0.01, 0.4)
    define_property("wind",       "float", 0.0, -2.0,  2.0)
    define_property("color_mode", "int",   0,    0,    2)
    --  0 = classic fire (black → red → orange → yellow → white)
    --  1 = cold fire    (black → blue → cyan → white)
    --  2 = toxic        (black → green → yellow → white)
end

-- ─── State ────────────────────────────────────────────────────────────────────

local heat    = {}    -- flat [y * width + x] heat buffer, values 0.0 .. 1.0
local W, H           -- cached dimensions

-- Build a flat index (0-based x/y)
local function idx(x, y)
    return y * W + x + 1   -- Lua tables are 1-based
end

-- ─── Colour palette ───────────────────────────────────────────────────────────

-- Map heat 0..1 to r,g,b using a 4-stop gradient
local palettes = {
    -- classic fire: black → red → orange → yellow → white
    [0] = {
        {0, 0, 0},          -- 0.0  black
        {180, 0, 0},        -- 0.35 deep red
        {255, 80, 0},       -- 0.6  orange
        {255, 220, 30},     -- 0.85 yellow
        {255, 255, 255},    -- 1.0  white
    },
    -- cold fire: black → deep blue → cyan → white
    [1] = {
        {0,   0,   0},
        {0,   0,  200},
        {0, 120, 255},
        {80, 220, 255},
        {255, 255, 255},
    },
    -- toxic: black → dark green → acid green → yellow → white
    [2] = {
        {0,   0,   0},
        {0,  80,   0},
        {40, 200,  0},
        {180, 255, 40},
        {255, 255, 255},
    },
}

local stops = {0.0, 0.35, 0.6, 0.85, 1.0}

local function heat_to_rgb(h, mode)
    local pal = palettes[mode] or palettes[0]
    -- find the two surrounding stops
    local lo, hi = 1, 2
    for i = 2, #stops do
        if h <= stops[i] then
            lo = i - 1
            hi = i
            break
        end
        lo = #stops - 1
        hi = #stops
    end
    local t = 0.0
    local denom = stops[hi] - stops[lo]
    if denom > 0 then
        t = (h - stops[lo]) / denom
    end
    local ca = pal[lo]
    local cb = pal[hi]
    local r = math.floor(ca[1] + (cb[1] - ca[1]) * t)
    local g = math.floor(ca[2] + (cb[2] - ca[2]) * t)
    local b = math.floor(ca[3] + (cb[3] - ca[3]) * t)
    return r, g, b
end

-- ─── Lifecycle ────────────────────────────────────────────────────────────────

function initialize()
    W = width
    H = height
    -- zero out heat buffer
    for i = 1, W * H do
        heat[i] = 0.0
    end
    log("lua_fire: initialized " .. W .. "x" .. H)
end

-- ─── Render ───────────────────────────────────────────────────────────────────

function render()
    local intensity = get_property("intensity")
    local cooling   = get_property("cooling")
    local wind      = get_property("wind")
    local mode      = math.floor(get_property("color_mode"))

    -- 1. Seed the bottom row with hot coals (randomised each frame)
    local base_y = H - 1
    for x = 0, W - 1 do
        -- Random embers: some cells max-heat, some flicker off
        local v = math.random()
        if v < intensity then
            heat[idx(x, base_y)] = 0.85 + math.random() * 0.15
        else
            heat[idx(x, base_y)] = heat[idx(x, base_y)] * 0.8
        end
    end

    -- 2. Also seed the second-to-last row for a thicker base
    for x = 0, W - 1 do
        if math.random() < intensity * 0.7 then
            heat[idx(x, base_y - 1)] =
                (heat[idx(x, base_y - 1)] + heat[idx(x, base_y)]) * 0.5
        end
    end

    -- 3. Propagate heat upward (iterate top-down so we don't re-use new values)
    --    Wind shifts the sample point horizontally by ±1 pixel on average.
    local wind_bias = math.floor(wind + 0.5)   -- -2..2 integer pixel shift

    for y = 0, H - 3 do
        for x = 0, W - 1 do
            -- Average heat from three cells one row below, shifted by wind
            local sum = 0.0
            local cnt = 0
            for dx = -1, 1 do
                local sx = x + dx + wind_bias
                if sx >= 0 and sx < W then
                    sum = sum + heat[idx(sx, y + 1)]
                    cnt = cnt + 1
                end
            end

            local avg = (cnt > 0) and (sum / cnt) or 0.0

            -- Cool down as heat rises
            local cool = math.random() * cooling
            heat[idx(x, y)] = math.max(0.0, avg - cool)
        end
    end

    -- 4. Draw heat buffer to pixels
    for y = 0, H - 1 do
        for x = 0, W - 1 do
            local h = heat[idx(x, y)]
            local r, g, b = heat_to_rgb(h, mode)
            set_pixel(x, y, r, g, b)
        end
    end

    return true
end
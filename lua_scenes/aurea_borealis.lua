-- aurora.lua
-- Animated aurora borealis effect with flowing curtains of colour.
-- Drop into lua_scenes/ and it will appear in the scene list automatically.

name = "lua_aurora"

-- ─── Setup ────────────────────────────────────────────────────────────────────

function setup()
    define_property("speed",      "float", 1.0,  0.1, 5.0)
    define_property("wave_scale", "float", 0.08, 0.01, 0.3)
    define_property("bands",      "int",   3,    1,    6)
    define_property("color_a",    "color", 0x00FF88)   -- teal/green
    define_property("color_b",    "color", 0x4400FF)   -- violet
    define_property("brightness", "float", 0.85, 0.1, 1.0)
end

-- ─── Helpers ──────────────────────────────────────────────────────────────────

-- Smooth hermite interpolation (0..1 → 0..1)
local function smoothstep(t)
    t = math.max(0.0, math.min(1.0, t))
    return t * t * (3.0 - 2.0 * t)
end

-- Linear interpolate between two values
local function lerp(a, b, t)
    return a + (b - a) * t
end

-- Unpack a 24-bit 0xRRGGBB integer into r, g, b in 0..255
local function unpack_color(raw)
    local r = math.floor(raw / 65536) % 256
    local g = math.floor(raw / 256)   % 256
    local b = raw % 256
    return r, g, b
end

-- A fast, repeatable pseudo-noise function using sine sums
-- Returns a value in roughly -1 .. 1
local function noise(x, y, z)
    local s = math.sin(x * 127.1 + y * 311.7 + z * 74.3) * 43758.5453
    return s - math.floor(s)   -- fractional part → 0..1, bias it:
end

-- Smooth multi-octave value noise (2 octaves, fast enough for 128×128)
local function fbm(x, y, z)
    local v  =  math.sin(x * 1.7 + y * 0.9 + z)             -- octave 1
              + math.sin(x * 3.4 + y * 1.8 + z * 2.1) * 0.5 -- octave 2
              + math.sin(x * 6.8 + y * 3.6 + z * 3.7) * 0.25-- octave 3
    -- normalise from [-1.75, 1.75] to [0, 1]
    return (v / 1.75 + 1.0) * 0.5
end

-- ─── State ────────────────────────────────────────────────────────────────────

local band_offsets = {}   -- per-band phase offsets for variety

function initialize()
    math.randomseed(12345)
    for i = 1, 6 do
        band_offsets[i] = math.random() * math.pi * 2.0
    end
    log("aurora: initialized")
end

-- ─── Render ───────────────────────────────────────────────────────────────────

function render()
    local spd    = get_property("speed")
    local scale  = get_property("wave_scale")
    local bands  = math.floor(get_property("bands"))
    local bright = get_property("brightness")

    local raw_a  = get_property("color_a")
    local raw_b  = get_property("color_b")
    local ar, ag, ab = unpack_color(raw_a)
    local br, bg, bb = unpack_color(raw_b)

    local t = time * spd

    -- Height fraction of the matrix where the aurora lives (top 60 %)
    local aurora_top    = 0.0
    local aurora_bottom = 0.65

    for y = 0, height - 1 do
        local fy = y / (height - 1)   -- 0 (top) → 1 (bottom)

        -- Only the upper portion of the display shows aurora glow
        local sky_fade = smoothstep(
            (aurora_bottom - fy) / (aurora_bottom - aurora_top)
        )

        for x = 0, width - 1 do
            local fx = x / (width - 1)

            -- Accumulate glow from each aurora band
            local glow = 0.0

            for i = 1, bands do
                local fi     = (i - 1) / math.max(bands - 1, 1)
                local offset = band_offsets[i]

                -- Curtain centre wanders horizontally with time
                local cx = 0.5
                    + 0.3 * math.sin(t * 0.31 + offset)
                    + 0.15 * math.sin(t * 0.17 + offset * 1.7 + fi * 2.1)

                -- Curtain width pulses slightly
                local band_w = 0.18 + 0.08 * math.sin(t * 0.23 + offset * 2.3)

                -- Horizontal envelope: Gaussian-like falloff from curtain centre
                local dx      = (fx - cx) / band_w
                local h_shape = math.exp(-dx * dx * 2.5)

                -- Vertical ripple along the curtain (the "rays")
                local ray = fbm(
                    fx  * scale * 6.0 + offset,
                    fy  * scale * 3.0,
                    t   * 0.4   + offset * 0.5
                )

                -- Bottom fade-out so curtain doesn't reach the ground harshly
                local v_env = smoothstep(1.0 - fy / aurora_bottom)
                            * smoothstep(fy * 8.0 + 0.01)  -- tiny fade at top

                glow = glow + h_shape * ray * v_env * sky_fade
            end

            -- Normalise glow to 0..1
            glow = math.min(glow / bands, 1.0) * bright

            -- Colour: blend between color_a and color_b using horizontal position
            -- plus a slow time shift for colour movement
            local blend = fbm(
                fx * 2.1 + t * 0.12,
                fy * 1.3,
                t  * 0.07
            )

            local r = math.floor(lerp(ar, br, blend) * glow)
            local g = math.floor(lerp(ag, bg, blend) * glow)
            local b = math.floor(lerp(ab, bb, blend) * glow)

            -- Add a faint star field in the dark sky above the aurora
            local star = 0
            if glow < 0.05 and fy < aurora_bottom then
                -- deterministic "star" based on pixel position
                local sv = math.sin(x * 97.3 + y * 53.7) * 1000.0
                sv = sv - math.floor(sv)
                if sv > 0.985 then
                    local twinkle = (math.sin(t * 3.0 + sv * 20.0) + 1.0) * 0.5
                    star = math.floor(180 * twinkle)
                end
            end

            set_pixel(x, y,
                math.max(r, star),
                math.max(g, star),
                math.max(b, star))
        end
    end

    return true
end
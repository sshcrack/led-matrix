-- ============================================================================
-- Scene: Ethereal Portal
-- Description: A swirling, mathematically warped fluid vortex using layered
--              sine waves, creating an intricate neon fractal-like pattern.
-- ============================================================================

name = "ethereal_portal"
-- external_render_only = true -- Flagged for heavy computation

-- ============================================================================
-- 1. Property Setup
-- ============================================================================
function setup()
    -- Timing and structural properties
    define_property("speed", "float", 1.0, 0.1, 5.0)
    define_property("complexity", "int", 4, 1, 6)
    define_property("zoom", "float", 3.0, 1.0, 10.0)

    -- Aesthetic palette (Returns 0xRRGGBB integers)
    define_property("color_primary", "color", 0xFF0055)   -- Neon Pink/Magenta
    define_property("color_secondary", "color", 0x00FFFF) -- Cyan

    -- Visual modifiers
    define_property("core_brightness", "float", 1.5, 0.0, 3.0)
end

-- ============================================================================
-- 2. State Initialization
-- ============================================================================
local cx, cy

function initialize()
    -- Calculate canvas center
    cx = width / 2
    cy = height / 2

    log("Ethereal Portal initialized. Canvas: " .. width .. "x" .. height)
end

-- Helper: Unpack 24-bit 0xRRGGBB integer into 0.0 to 1.0 float ranges
local function hex_to_rgb_floats(hex)
    local r = (math.floor(hex / 65536) % 256) / 255.0
    local g = (math.floor(hex / 256) % 256) / 255.0
    local b = (hex % 256) / 255.0
    return r, g, b
end

-- ============================================================================
-- 3. Render Loop
-- ============================================================================
function render()
    -- Safely retrieve and coerce properties
    local speed = get_property("speed")
    local iters = math.floor(get_property("complexity")) -- Guard against float returns
    local zoom = get_property("zoom")
    local core_brightness = get_property("core_brightness")

    local pr, pg, pb = hex_to_rgb_floats(get_property("color_primary"))
    local sr, sg, sb = hex_to_rgb_floats(get_property("color_secondary"))

    local t = time * speed

    -- Pre-calculate rotation matrix for the vortex swirl
    local cos_t = math.cos(t * 0.2)
    local sin_t = math.sin(t * 0.2)

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            -- 1. Normalize coordinates (-1.0 to 1.0)
            local nx = (x - cx) / cx
            local ny = (y - cy) / cy

            -- Calculate distance from center for radial falloff/vignette
            local dist = math.sqrt(nx * nx + ny * ny)

            -- Apply slow global vortex rotation
            local rx = (nx * cos_t - ny * sin_t) * zoom
            local ry = (nx * sin_t + ny * cos_t) * zoom

            -- 2. Domain Warping / Accumulation loop
            local v = 0.0
            local px, py = rx, ry

            for i = 1, iters do
                local scale = i * 1.5

                -- Shift coordinates based on underlying sine functions (Fluid dynamics illusion)
                local dx = math.sin(py * scale + t)
                local dy = math.cos(px * scale - t * 0.8)

                px = px + dx * 0.4
                py = py + dy * 0.4

                -- Accumulate detail
                v = v + (math.sin(px * scale) * math.cos(py * scale)) / scale
            end

            -- 3. Shape the accumulated value
            v = math.abs(v)            -- Creates nice sharp banding lines where the waves cross 0
            v = math.min(1.0, v * 1.2) -- Boost contrast slightly

            -- Fade edges into darkness (Vignette)
            local falloff = math.max(0.0, 1.0 - (dist * 0.85))
            local mix_factor = v * falloff

            -- 4. Color Mapping (Non-linear blending)
            -- Black -> Primary Color -> Secondary Color
            local curve1 = math.sin(mix_factor * math.pi) -- Peaks at medium intensity
            local curve2 = mix_factor ^ 2.0               -- Peaks at high intensity

            local fr = (curve1 * pr) + (curve2 * sr)
            local fg = (curve1 * pg) + (curve2 * sg)
            local fb = (curve1 * pb) + (curve2 * sb)

            -- 5. Add an ethereal glowing core in the center
            local core = math.max(0.0, 0.25 - dist) * core_brightness
            core = core * (0.8 + 0.2 * math.sin(t * 4.0)) -- Pulse the core

            fr = fr + core
            fg = fg + core
            fb = fb + core

            -- 6. Convert back to 0-255 integer limits and draw
            local out_r = math.floor(math.min(1.0, math.max(0.0, fr)) * 255)
            local out_g = math.floor(math.min(1.0, math.max(0.0, fg)) * 255)
            local out_b = math.floor(math.min(1.0, math.max(0.0, fb)) * 255)

            set_pixel(x, y, out_r, out_g, out_b)
        end
    end

    -- Mandatory return contract
    return true
end

-- lua_scenes/rainbow_spiral.lua
-- A slowly-rotating spiral whose hue and speed can be tweaked at runtime.

name = "rainbow_spiral"

-------------------------------------------------
-- 1.  Declare tweakable properties
-------------------------------------------------
function setup()
    define_property("speed", "float", 0.8, 0.1, 3.0)   -- rotation speed
    define_property("arms",   "int", 3, 1, 8)          -- number of spiral arms
    define_property("tightness", "float", 3.0, 1.0, 8.0)
    define_property("brightness", "float", 1.0, 0.2, 1.0)
    define_property("bg_tint", "color", 0x000010)      -- deep-blue background
end

-------------------------------------------------
-- 2.  One-time setup when scene becomes visible
-------------------------------------------------
function initialize()
    -- nothing special needed here
end

-------------------------------------------------
-- 3.  Per-frame drawing
-------------------------------------------------
function render()
    clear()                       -- start with black canvas

    -- unpack background tint
    local raw_bg = get_property("bg_tint")
    local bg_r = math.floor(raw_bg / 65536) % 256
    local bg_g = math.floor(raw_bg / 256)   % 256
    local bg_b = raw_bg % 256
    -- fill canvas with tinted background
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            --- set_pixel(x, y, bg_r, bg_g, bg_b)
        end
    end

    -- read live properties
    local speed       = get_property("speed")
    local arms        = math.floor(get_property("arms"))
    local tightness   = get_property("tightness")
    local brightness  = get_property("brightness")

    local cx = width  / 2
    local cy = height / 2
    local max_r = math.min(cx, cy)

    -- precompute angle offset that grows with time
    local angle_shift = speed * time * 2.0 * math.pi

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local dx = x - cx
            local dy = y - cy
            local r = math.sqrt(dx*dx + dy*dy)
            if r <= max_r then
                local angle = math.atan(dy, dx) + angle_shift
                local spiral = angle * arms - r / tightness
                local hue = (spiral / (2 * math.pi)) % 1
                local sat = 1.0
                local val = brightness * (1.0 - r / max_r)  -- fade at edges

                -- fast HSV→RGB (simplified)
                local h6 = hue * 6
                local c = val * sat
                local x1 = c * (1 - math.abs((h6 % 2) - 1))
                local m = val - c
                local r_, g_, b_
                if     h6 < 1 then r_, g_, b_ = c, x1, 0
                elseif h6 < 2 then r_, g_, b_ = x1, c, 0
                elseif h6 < 3 then r_, g_, b_ = 0, c, x1
                elseif h6 < 4 then r_, g_, b_ = 0, x1, c
                elseif h6 < 5 then r_, g_, b_ = x1, 0, c
                else                r_, g_, b_ = c, 0, x1
                end
                local R = math.floor((r_ + m) * 255)
                local G = math.floor((g_ + m) * 255)
                local B = math.floor((b_ + m) * 255)
                set_pixel(x, y, R, G, B)
            end
        end
    end

    return true  -- keep scene alive
end
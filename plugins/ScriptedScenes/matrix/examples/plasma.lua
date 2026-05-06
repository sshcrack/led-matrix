-- Plasma wave effect
-- A classic demo-scene style plasma that cycles through colours.
name = "lua_plasma"

function setup()
    define_property("speed", "float", 1.0, 0.1, 5.0)
    define_property("scale", "float", 0.15, 0.05, 0.5)
end

function render()
    local t    = time * get_property("speed")
    local sc   = get_property("scale")

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local v = math.sin(x * sc + t)
                    + math.sin(y * sc + t * 0.7)
                    + math.sin((x + y) * sc * 0.5 + t * 1.3)

            -- Map -3..3 → 0..1
            local norm = (v + 3.0) / 6.0

            -- HSV-style hue rotation: split into R/G/B phases
            local phase = norm * math.pi * 2.0
            local r = math.floor((math.sin(phase)           + 1.0) * 127)
            local g = math.floor((math.sin(phase + 2.094)   + 1.0) * 127)
            local b = math.floor((math.sin(phase + 4.189)   + 1.0) * 127)

            set_pixel(x, y, r, g, b)
        end
    end
    return true
end

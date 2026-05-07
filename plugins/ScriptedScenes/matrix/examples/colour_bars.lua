-- Bouncing colour bars
name = "lua_colour_bars"

function setup()
    define_property("bar_count", "int",   8,   1, 32)
    define_property("speed",     "float", 1.0, 0.1, 5.0)
end

function render()
    local bars  = get_property("bar_count")
    local spd   = get_property("speed")
    local t     = time * spd

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            -- Which bar column is this pixel in?
            local bar   = math.floor(x / width * bars)
            local phase = bar / bars * math.pi * 2.0 + t

            local r = math.floor((math.sin(phase)           + 1.0) * 127)
            local g = math.floor((math.sin(phase + 2.094)   + 1.0) * 127)
            local b = math.floor((math.sin(phase + 4.189)   + 1.0) * 127)

            set_pixel(x, y, r, g, b)
        end
    end
    return true
end

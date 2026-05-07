-- Starfield / warp-speed effect
name = "lua_starfield"

local stars = {}
local NUM_STARS = 80

function setup()
    define_property("speed", "float", 0.5, 0.05, 3.0)
end

function initialize()
    math.randomseed(42)
    stars = {}
    for i = 1, NUM_STARS do
        stars[i] = {
            x  = (math.random() - 0.5) * width,
            y  = (math.random() - 0.5) * height,
            z  = math.random(),   -- depth 0..1
        }
    end
end

function render()
    clear()
    local spd = get_property("speed")
    local cx  = width  / 2
    local cy  = height / 2

    for i = 1, NUM_STARS do
        local s = stars[i]
        -- Move star toward viewer (z decreases)
        s.z = s.z - dt * spd
        if s.z <= 0 then
            s.x = (math.random() - 0.5) * width
            s.y = (math.random() - 0.5) * height
            s.z = 1.0
        end

        -- Project 3-D position onto 2-D screen
        local sx = math.floor(s.x / s.z + cx)
        local sy = math.floor(s.y / s.z + cy)

        -- Brightness: bright when close (small z)
        local bright = math.floor((1.0 - s.z) * 255)

        set_pixel(sx, sy, bright, bright, bright)
    end
    return true
end

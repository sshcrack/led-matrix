-- Ripple / pond-drop effect
-- Concentric rings radiate from a point that bounces around the canvas.
name = "lua_ripple"

local cx, cy   -- current centre of the ripple
local vx, vy   -- velocity of the centre (pixels / second)

function setup()
    define_property("speed",     "float", 1.0, 0.1, 5.0)
    define_property("frequency", "float", 0.3, 0.05, 1.0)
    define_property("tint",      "color", 0x00BFFF)
end

function initialize()
    math.randomseed(42)
    cx = width  / 2
    cy = height / 2
    vx = 18 + math.random() * 14  -- ~18-32 px/s
    vy = 14 + math.random() * 14
end

function render()
    local spd  = get_property("speed")
    local freq = get_property("frequency")
    local raw  = get_property("tint")
    local tr   = math.floor(raw / 65536) % 256
    local tg   = math.floor(raw / 256)   % 256
    local tb   =            raw           % 256

    -- Move the centre, bounce off walls
    cx = cx + vx * dt
    cy = cy + vy * dt
    if cx < 0        then cx = -cx;               vx = -vx end
    if cx > width-1  then cx = 2*(width-1)  - cx; vx = -vx end
    if cy < 0        then cy = -cy;               vy = -vy end
    if cy > height-1 then cy = 2*(height-1) - cy; vy = -vy end

    local t = time * spd

    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local dx = x - cx
            local dy = y - cy
            local dist = math.sqrt(dx*dx + dy*dy)

            -- Cosine wave that decays with distance
            local wave   = math.cos(dist * freq * math.pi * 2 - t * 6)
            local decay  = math.max(0, 1 - dist / (width * 0.6))
            local bright = (wave + 1) * 0.5 * decay

            local r = math.floor(tr * bright)
            local g = math.floor(tg * bright)
            local b = math.floor(tb * bright)
            set_pixel(x, y, r, g, b)
        end
    end
    return true
end

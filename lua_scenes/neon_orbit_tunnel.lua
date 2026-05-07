name = "neon_orbit_tunnel"

local stars = {}
local palette = {}
local center_x = 0
local center_y = 0

local function clamp(v)
    if v < 0 then
        return 0
    elseif v > 255 then
        return 255
    end
    return math.floor(v)
end

local function hsv_to_rgb(h, s, v)
    local i = math.floor(h * 6)
    local f = h * 6 - i
    local p = v * (1 - s)
    local q = v * (1 - f * s)
    local t = v * (1 - (1 - f) * s)

    local r, g, b

    i = i % 6

    if i == 0 then
        r, g, b = v, t, p
    elseif i == 1 then
        r, g, b = q, v, p
    elseif i == 2 then
        r, g, b = p, v, t
    elseif i == 3 then
        r, g, b = p, q, v
    elseif i == 4 then
        r, g, b = t, p, v
    else
        r, g, b = v, p, q
    end

    return clamp(r * 255), clamp(g * 255), clamp(b * 255)
end

function setup()
    define_property("rotation_speed", "float", 1.2, 0.1, 8.0)
    define_property("tunnel_depth", "float", 2.5, 0.5, 8.0)
    define_property("star_count", "int", 90, 10, 240)
    define_property("trail_strength", "float", 0.85, 0.5, 0.98)
    define_property("rainbow_mode", "bool", true)
    define_property("base_color", "color", 0x00D4FF)
end

function initialize()
    math.randomseed(1337)

    center_x = width / 2
    center_y = height / 2

    palette = {}

    for i = 0, 255 do
        local r, g, b = hsv_to_rgb(i / 255.0, 1.0, 1.0)
        palette[i] = { r = r, g = g, b = b }
    end

    stars = {}

    local count = math.floor(get_property("star_count"))

    for i = 1, count do
        stars[i] = {
            angle = math.random() * math.pi * 2,
            radius = math.random(),
            speed = 0.2 + math.random() * 1.8,
            twist = 0.5 + math.random() * 2.0,
            brightness = 0.4 + math.random() * 0.6
        }
    end

    log("Initialized neon_orbit_tunnel with " .. tostring(count) .. " stars")
end

local function unpack_color(raw)
    local r = math.floor(raw / 65536) % 256
    local g = math.floor(raw / 256) % 256
    local b = raw % 256
    return r, g, b
end

function render()
    local trail = get_property("trail_strength")
    local rotation_speed = get_property("rotation_speed")
    local tunnel_depth = get_property("tunnel_depth")
    local rainbow_mode = get_property("rainbow_mode")

    local base_r, base_g, base_b = unpack_color(get_property("base_color"))

    clear()

    local ring_count = 18

    for ring = 1, ring_count do
        local z = (ring / ring_count)
        local pulse = 0.5 + 0.5 * math.sin(time * 1.4 + z * 12)

        local radius = (z ^ 1.8) * width * 0.55
        local rotation = time * rotation_speed * (0.2 + z)

        for segment = 0, 95 do
            local a = (segment / 96.0) * math.pi * 2 + rotation

            local wobble = math.sin(a * 3 + time * 2 + z * 8) * 4

            local x = math.floor(center_x + math.cos(a) * (radius + wobble))
            local y = math.floor(center_y + math.sin(a) * (radius + wobble))

            local intensity = (1.0 - z) * pulse * trail

            local r, g, b

            if rainbow_mode then
                local color_index = math.floor((segment * 2 + time * 80 + z * 255) % 255)
                local c = palette[color_index]

                r = clamp(c.r * intensity)
                g = clamp(c.g * intensity)
                b = clamp(c.b * intensity)
            else
                r = clamp(base_r * intensity)
                g = clamp(base_g * intensity)
                b = clamp(base_b * intensity)
            end

            set_pixel(x, y, r, g, b)
        end
    end

    for i = 1, #stars do
        local star = stars[i]

        star.radius = star.radius + dt * star.speed * tunnel_depth * 0.15
        star.angle = star.angle + dt * rotation_speed * star.twist

        if star.radius > 1.2 then
            star.radius = 0.05
            star.angle = math.random() * math.pi * 2
        end

        local spiral = star.radius * width * 0.7

        local x = math.floor(center_x + math.cos(star.angle) * spiral)
        local y = math.floor(center_y + math.sin(star.angle) * spiral)

        local brightness = (1.2 - star.radius) * 255 * star.brightness

        local r, g, b

        if rainbow_mode then
            local color_index = math.floor((star.angle * 40 + time * 120) % 255)
            local c = palette[color_index]

            r = clamp(c.r * brightness / 255)
            g = clamp(c.g * brightness / 255)
            b = clamp(c.b * brightness / 255)
        else
            r = clamp(base_r * brightness / 255)
            g = clamp(base_g * brightness / 255)
            b = clamp(base_b * brightness / 255)
        end

        set_pixel(x, y, r, g, b)

        local tail_x = math.floor(center_x + math.cos(star.angle - 0.08) * (spiral - 2))
        local tail_y = math.floor(center_y + math.sin(star.angle - 0.08) * (spiral - 2))

        set_pixel(
            tail_x,
            tail_y,
            clamp(r * 0.4),
            clamp(g * 0.4),
            clamp(b * 0.4)
        )
    end

    return true
end
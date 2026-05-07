-- 1. Script Contract
name = "realistic_cloth"

-- Local state variables
local points = {}
local links = {}
local prev_dt = 0.016
local cols = 14
local rows = 14

-- 2. Setup: Declare properties
function setup()
    define_property("gravity", "float", 400.0, 50.0, 1000.0)
    define_property("wind_strength", "float", 100.0, 0.0, 500.0)
    define_property("stiffness", "int", 6, 1, 15) -- Number of constraint iterations
    define_property("cloth_color", "color", 0x00FF88) -- Spring green
    define_property("sphere_color", "color", 0xFF2255) -- Crimson
    define_property("draw_sphere", "bool", true)
end

-- Helper: Bresenham's Line Algorithm to draw the cloth links
local function draw_line(x0, y0, x1, y1, r, g, b)
    x0 = math.floor(x0 + 0.5)
    y0 = math.floor(y0 + 0.5)
    x1 = math.floor(x1 + 0.5)
    y1 = math.floor(y1 + 0.5)
    
    local dx = math.abs(x1 - x0)
    local dy = -math.abs(y1 - y0)
    local sx = x0 < x1 and 1 or -1
    local sy = y0 < y1 and 1 or -1
    local err = dx + dy
    
    while true do
        set_pixel(x0, y0, r, g, b)
        if x0 == x1 and y0 == y1 then break end
        local e2 = 2 * err
        if e2 >= dy then
            err = err + dy
            x0 = x0 + sx
        end
        if e2 <= dx then
            err = err + dx
            y0 = y0 + sy
        end
    end
end

-- Helper: Draw a solid circle for the collision object
local function draw_solid_circle(cx, cy, radius, r, g, b)
    for y = -radius, radius do
        for x = -radius, radius do
            if x*x + y*y <= radius*radius then
                set_pixel(math.floor(cx + x), math.floor(cy + y), r, g, b)
            end
        end
    end
end

-- 3. Initialize: Reset simulation state
function initialize()
    points = {}
    links = {}
    prev_dt = 0.016
    
    -- Calculate cloth dimensions relative to canvas
    local spacing = math.floor(width / (cols + 4))
    local start_x = (width - (cols - 1) * spacing) / 2
    local start_y = 5
    
    -- Generate particles
    for y = 0, rows - 1 do
        for x = 0, cols - 1 do
            local px = start_x + x * spacing
            local py = start_y + y * spacing
            table.insert(points, {
                x = px, y = py,
                ox = px, oy = py, -- Old x, y for Verlet velocity
                pinned = (y == 0) -- Pin the top row
            })
        end
    end
    
    -- Generate constraints (structural links)
    local function get_idx(x, y) return y * cols + x + 1 end
    
    for y = 0, rows - 1 do
        for x = 0, cols - 1 do
            if x < cols - 1 then
                table.insert(links, { p1 = get_idx(x, y), p2 = get_idx(x+1, y), rest = spacing })
            end
            if y < rows - 1 then
                table.insert(links, { p1 = get_idx(x, y), p2 = get_idx(x, y+1), rest = spacing })
            end
        end
    end
    
    log("Cloth physics initialized. Nodes: " .. tostring(#points) .. ", Links: " .. tostring(#links))
end

-- 4. Render: Called every frame
function render()
    clear()
    
    -- Property extraction
    local gravity = get_property("gravity")
    local wind_strength = get_property("wind_strength")
    local stiffness = math.floor(get_property("stiffness"))
    local raw_cloth = get_property("cloth_color")
    local raw_sphere = get_property("sphere_color")
    
    -- Color unpacking
    local cr = math.floor(raw_cloth / 65536) % 256
    local cg = math.floor(raw_cloth / 256) % 256
    local cb = raw_cloth % 256
    local sr = math.floor(raw_sphere / 65536) % 256
    local sg = math.floor(raw_sphere / 256) % 256
    local sb = raw_sphere % 256

    -- Cap delta time to prevent physics explosions during lag spikes
    local safe_dt = math.min(dt, 0.05)
    if prev_dt == 0 then prev_dt = safe_dt end
    local dt_ratio = safe_dt / prev_dt

    -- Dynamic variables
    local wind_x = math.sin(time * 1.5) * math.cos(time * 0.8) * wind_strength
    local wind_y = math.sin(time * 2.1) * (wind_strength * 0.2)
    
    -- Kinematic Sphere (Moves around in a figure-8)
    local sphere_r = 20.0
    local sphere_x = (width / 2) + math.sin(time) * (width / 3)
    local sphere_y = (height / 2) + math.sin(time * 2.0) * (height / 4) + 15

    -- Render the sphere if requested
    if get_property("draw_sphere") then
        draw_solid_circle(sphere_x, sphere_y, sphere_r, sr, sg, sb)
    end

    -- --- PHYSICS INTEGRATION (Time-Corrected Verlet) ---
    for i, p in ipairs(points) do
        if not p.pinned then
            -- Calculate forces
            local acc_x = wind_x
            local acc_y = gravity + wind_y
            
            -- Velocity derived from previous frame (scaled by dt_ratio for framerate independence)
            local vx = (p.x - p.ox) * dt_ratio * 0.99 -- 0.99 is air friction
            local vy = (p.y - p.oy) * dt_ratio * 0.99
            
            p.ox = p.x
            p.oy = p.y
            
            -- Apply motion
            p.x = p.x + vx + acc_x * (safe_dt * safe_dt)
            p.y = p.y + vy + acc_y * (safe_dt * safe_dt)
            
            -- Sphere Collision Detection
            local dx = p.x - sphere_x
            local dy = p.y - sphere_y
            local dist2 = dx*dx + dy*dy
            
            if dist2 < sphere_r * sphere_r then
                local dist = math.sqrt(dist2)
                local overlap = sphere_r - dist
                
                -- Push particle out of the sphere
                local nx = dx / dist
                local ny = dy / dist
                p.x = p.x + nx * overlap
                p.y = p.y + ny * overlap
                
                -- Friction against the sphere (dampen stored velocity)
                p.ox = p.x - (p.x - p.ox) * 0.5 
            end
            
            -- Floor / Wall constraints
            if p.y > height - 1 then p.y = height - 1 end
            if p.x < 0 then p.x = 0 end
            if p.x > width - 1 then p.x = width - 1 end
        end
    end
    
    -- --- CONSTRAINTS RESOLUTION (Hooke's Law approximation) ---
    -- Multiple iterations stiffen the cloth and distribute forces
    for iter = 1, stiffness do
        for i, link in ipairs(links) do
            local p1 = points[link.p1]
            local p2 = points[link.p2]
            
            local dx = p2.x - p1.x
            local dy = p2.y - p1.y
            local dist = math.sqrt(dx*dx + dy*dy)
            
            -- Resolve spring length difference
            if dist > 0.0001 then
                local difference = (dist - link.rest) / dist
                local offset_x = dx * 0.5 * difference
                local offset_y = dy * 0.5 * difference
                
                if not p1.pinned then
                    p1.x = p1.x + offset_x
                    p1.y = p1.y + offset_y
                end
                
                if not p2.pinned then
                    p2.x = p2.x - offset_x
                    p2.y = p2.y - offset_y
                end
            end
        end
    end
    
    -- Update previous delta time for next frame's Verlet calculations
    prev_dt = safe_dt

    -- --- RENDERING ---
    -- Draw the cloth links using Bresenham's line algorithm
    for i, link in ipairs(links) do
        local p1 = points[link.p1]
        local p2 = points[link.p2]
        draw_line(p1.x, p1.y, p2.x, p2.y, cr, cg, cb)
    end
    
    return true
end
-- ============================================================================
-- File: plasma_waves.lua
-- Location: <working directory>/lua_scenes/plasma_waves.lua
-- ============================================================================

-- 1. Script Contract: Required unique global name (no spaces)
name = "plasma_waves"

-- 2. Setup: Called once. Strictly used for defining properties.
function setup()
    -- define_property(name, type, default [, min, max])
    define_property("speed", "float", 2.0, 0.1, 10.0)
    define_property("scale", "float", 15.0, 1.0, 50.0)
    
    -- Int property (will need math.floor during extraction)
    define_property("complexity", "int", 3, 1, 4)
    
    -- Color property (defaults to Cyan: 0x00FFFF)
    define_property("tint", "color", 0x00FFFF)
end

-- 3. Initialize: Called when first displayed. 
function initialize()
    -- Good place to log setup and reset states
    log("Plasma Waves initialized! Canvas size: " .. width .. "x" .. height)
end

-- 4. Render: Called every frame. Must return true.
function render()
    -- Rapidly fill the canvas with black
    clear()

    -- Retrieve properties
    local speed = get_property("speed")
    local scale = get_property("scale")
    
    -- Pitfall Prevention: Force int property into a Lua integer
    local complexity = math.floor(get_property("complexity"))

    -- Pitfall Prevention: Unpack the 24-bit color integer manually
    local raw_color = get_property("tint")
    local base_r = math.floor(raw_color / 65536) % 256
    local base_g = math.floor(raw_color / 256) % 256
    local base_b = raw_color % 256

    -- Calculate current animation phase based on elapsed time and speed
    local t = time * speed

    -- Loop through every pixel (0 to width-1, 0 to height-1)
    for x = 0, width - 1 do
        for y = 0, height - 1 do
            
            -- Coordinate math based on scale
            local nx = x / scale
            local ny = y / scale
            
            -- Accumulate sine waves for a classic "plasma" effect
            local v = 0
            
            -- Layer 1 & 2: Simple moving directional waves
            if complexity >= 1 then v = v + math.sin(nx + t) end
            if complexity >= 2 then v = v + math.sin(ny - t) end
            -- Layer 3: Diagonal wave
            if complexity >= 3 then v = v + math.sin((nx + ny + t) / 2.0) end
            -- Layer 4: Circular ripples from the origin
            if complexity >= 4 then v = v + math.sin(math.sqrt(nx*nx + ny*ny) - t) end
            
            -- Normalize the accumulated sine value (roughly between 0.0 and 1.0)
            local normalized = (v + complexity) / (complexity * 2.0)
            
            -- Modulate the user's chosen base color using the math
            local r = math.floor(base_r * normalized)
            local g = math.floor(base_g * math.abs(math.sin(normalized * math.pi)))
            local b = math.floor(base_b * (1.0 - normalized))
            
            -- Safely clamp colors between 0 and 255
            r = math.max(0, math.min(255, r))
            g = math.max(0, math.min(255, g))
            b = math.max(0, math.min(255, b))

            -- Draw the pixel
            set_pixel(x, y, r, g, b)
        end
    end

    -- Required: Must end with return true or the scene will terminate
    return true
end
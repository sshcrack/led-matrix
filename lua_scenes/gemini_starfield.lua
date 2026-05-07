-- 1. Script Contract: Required global name (unique, no spaces)
name = "starfield_warp"

-- Local state variables to hold our scene data
local stars = {}

-- 2. Setup: Called once. Strictly used for defining properties.
function setup()
    -- define_property(name, type, default, min, max)
    define_property("star_count", "int", 150, 10, 500)
    define_property("speed", "float", 80.0, 10.0, 300.0)
    define_property("warp_color", "color", 0x00FFFF) -- Cyan by default
    define_property("wobble_camera", "bool", true)
    define_property("clear_bg", "bool", true)
end

-- 3. Initialize: Called when scene is first displayed or re-initialized.
function initialize()
    -- Reset state
    stars = {}
    
    -- NOTE: get_property("int") returns a float, so we MUST wrap in math.floor()
    local count = math.floor(get_property("star_count"))
    
    -- Populate initial stars
    for i = 1, count do
        table.insert(stars, {
            x = (math.random() * width) - (width / 2),
            y = (math.random() * height) - (height / 2),
            z = math.random() * width -- Depth
        })
    end
    
    log("Starfield scene initialized with " .. tostring(count) .. " stars.")
end

-- 4. Render: Called every frame. Must end with return true.
function render()
    -- Clear canvas rapidly with black if property is true
    if get_property("clear_bg") then
        clear()
    end

    -- Retrieve configured properties
    local speed = get_property("speed")
    local raw_color = get_property("warp_color")
    local wobble = get_property("wobble_camera")
    
    -- 5. Critical Pitfall: Color Extraction
    local r = math.floor(raw_color / 65536) % 256
    local g = math.floor(raw_color / 256) % 256
    local b = raw_color % 256

    -- Calculate screen center (using `width` and `height` globals)
    local cx = width / 2
    local cy = height / 2
    
    -- Add dynamic movement using the `time` global
    if wobble then
        cx = cx + (math.sin(time) * (width / 8))
        cy = cy + (math.cos(time * 0.8) * (height / 8))
    end
    
    cx = math.floor(cx)
    cy = math.floor(cy)

    -- Update and draw each star
    for i, star in ipairs(stars) do
        -- Move star closer to camera using `dt` (delta time)
        star.z = star.z - (speed * dt)
        
        -- If star passes the camera (z <= 0), respawn it far away
        if star.z <= 0 then
            star.z = width
            star.x = (math.random() * width) - (width / 2)
            star.y = (math.random() * height) - (height / 2)
        end
        
        -- Project 3D coordinates to 2D screen space
        -- math.max prevents divide-by-zero crashes
        local pz = math.max(star.z, 0.001) 
        local px = math.floor((star.x / pz) * width + cx)
        local py = math.floor((star.y / pz) * width + cy)
        
        -- Fade stars in as they get closer (intensity 0.0 to 1.0)
        local intensity = 1.0 - (star.z / width)
        if intensity < 0 then intensity = 0 end
        if intensity > 1 then intensity = 1 end
        
        local final_r = math.floor(r * intensity)
        local final_g = math.floor(g * intensity)
        local final_b = math.floor(b * intensity)
        
        -- Draw the pixel (Out-of-bounds coordinates are safely ignored by the API)
        set_pixel(px, py, final_r, final_g, final_b)
    end
    
    -- 6. Script Contract: MUST return true
    return true
end
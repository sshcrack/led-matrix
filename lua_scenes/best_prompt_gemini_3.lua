-- ============================================================================
-- THE HYPERCOMPLEX QUATERNION RAYMARCHER (v3 - Studio Polish)
-- ============================================================================
-- By your resident Mathematical Visualization Maestro.
-- This version implements pipeline-accurate sRGB spatial dithering to 
-- eliminate all banding and Moire interference patterns, resulting in a 
-- flawless, film-grade hypercomplex render.
-- ============================================================================

name = "expert_quaternion_julia"

function setup()
    -- Note: Properties only update on server restart, but the logic updates instantly!
    define_property("max_steps", "int", 60, 10, 150) -- Increased default for better detail
    define_property("fractal_iters", "int", 7, 2, 15)
    define_property("time_scale", "float", 0.4, 0.0, 3.0)
    define_property("camera_dist", "float", 2.2, 0.5, 5.0)
    define_property("glow_color", "color", 0x00D9FF)   -- Electric Cyan
    define_property("surface_color", "color", 0xFF007A) -- Deep Pink/Magenta
    define_property("light_dir_x", "float", 0.8, -1.0, 1.0)
    define_property("light_dir_y", "float", 1.0, -1.0, 1.0)
    define_property("light_dir_z", "float", -0.5, -1.0, 1.0)
end

function initialize()
    math.randomseed(7777777)
    log("INITIALIZED EXPERT QUATERNION JULIA SET (Studio Polish Version).")
end

-- ----------------------------------------------------------------------------
-- CORE MATHEMATICAL OPERATIONS 
-- ----------------------------------------------------------------------------

local function normalize(x, y, z)
    local len_sq = x*x + y*y + z*z
    if len_sq == 0.0 then return 0.0, 0.0, 0.0 end
    local inv_len = 1.0 / math.sqrt(len_sq)
    return x * inv_len, y * inv_len, z * inv_len
end

local function cross(ax, ay, az, bx, by, bz)
    return ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx
end

-- Distance Estimator for 4D Quaternion Julia Set
local function map_distance(px, py, pz, cw, cx, cy, cz, iters)
    local zw, zx, zy, zz = px, py, pz, 0.0
    local dz2 = 1.0  
    local r2 = 0.0   
    
    for i = 1, iters do
        r2 = zw*zw + zx*zx + zy*zy + zz*zz
        if r2 > 15.0 then break end 
        
        dz2 = 4.0 * r2 * dz2
        
        local new_w = zw*zw - zx*zx - zy*zy - zz*zz + cw
        local new_x = 2.0 * zw * zx + cx
        local new_y = 2.0 * zw * zy + cy
        local new_z = 2.0 * zw * zz + cz
        
        zw, zx, zy, zz = new_w, new_x, new_y, new_z
    end
    
    if r2 < 1e-8 then r2 = 1e-8 end
    if dz2 < 1e-8 then dz2 = 1e-8 end
    
    return 0.25 * math.log(r2) * math.sqrt(r2 / dz2)
end

-- ----------------------------------------------------------------------------
-- MAIN RENDER LOOP
-- ----------------------------------------------------------------------------
function render()
    local max_steps = math.floor(get_property("max_steps"))
    local f_iters = math.floor(get_property("fractal_iters"))
    local t_scale = get_property("time_scale")
    local cam_dist = get_property("camera_dist")
    
    local raw_glow = get_property("glow_color")
    local glow_r = (math.floor(raw_glow / 65536) % 256) / 255.0
    local glow_g = (math.floor(raw_glow / 256) % 256) / 255.0
    local glow_b = (raw_glow % 256) / 255.0

    local raw_surf = get_property("surface_color")
    local surf_r = (math.floor(raw_surf / 65536) % 256) / 255.0
    local surf_g = (math.floor(raw_surf / 256) % 256) / 255.0
    local surf_b = (raw_surf % 256) / 255.0
    
    local lx, ly, lz = normalize(
        get_property("light_dir_x"),
        get_property("light_dir_y"),
        get_property("light_dir_z")
    )

    -- Animate Quaternion Seed
    local sim_time = time * t_scale
    local cw = math.sin(sim_time * 0.45) * 0.45
    local cx = math.cos(sim_time * 0.32) * 0.45
    local cy = math.sin(sim_time * 0.71) * 0.45
    local cz = math.cos(sim_time * 0.19) * 0.45

    -- Camera Matrix Setup
    local cam_x = math.sin(sim_time * 0.2) * cam_dist
    local cam_y = math.sin(sim_time * 0.15) * (cam_dist * 0.6)
    local cam_z = math.cos(sim_time * 0.2) * cam_dist

    local fw_x, fw_y, fw_z = normalize(-cam_x, -cam_y, -cam_z)
    local rt_x, rt_y, rt_z = normalize(cross(fw_x, fw_y, fw_z, 0.0, 1.0, 0.0))
    local up_x, up_y, up_z = cross(rt_x, rt_y, rt_z, fw_x, fw_y, fw_z)

    local half_w, half_h = width * 0.5, height * 0.5
    local inv_h = 1.0 / height
    local zoom = 1.2
    local hit_threshold = 0.002
    local normal_eps = 0.002

    clear()

    for y = 0, height - 1 do
        local uv_y = -(y - half_h) * inv_h * 2.0
        
        for x = 0, width - 1 do
            local uv_x = (x - half_w) * inv_h * 2.0
            
            local rd_x = fw_x * zoom + rt_x * uv_x + up_x * uv_y
            local rd_y = fw_y * zoom + rt_y * uv_x + up_y * uv_y
            local rd_z = fw_z * zoom + rt_z * uv_x + up_z * uv_y
            rd_x, rd_y, rd_z = normalize(rd_x, rd_y, rd_z)

            local t = 0.0
            local dist = 0.0
            local glow_accum = 0.0
            local hit = false
            local px, py, pz = 0.0, 0.0, 0.0

            for s = 1, max_steps do
                px = cam_x + rd_x * t
                py = cam_y + rd_y * t
                pz = cam_z + rd_z * t
                
                dist = map_distance(px, py, pz, cw, cx, cy, cz, f_iters)
                
                -- Accumulate volumetric glow
                glow_accum = glow_accum + 0.03 / (1.0 + dist * dist * 40.0)
                
                if dist < hit_threshold then
                    hit = true
                    break
                end
                
                t = t + dist
                if t > 6.0 then break end 
            end

            local final_r, final_g, final_b = 0.0, 0.0, 0.0
            
            if hit then
                local nx = map_distance(px + normal_eps, py, pz, cw, cx, cy, cz, f_iters) - 
                           map_distance(px - normal_eps, py, pz, cw, cx, cy, cz, f_iters)
                local ny = map_distance(px, py + normal_eps, pz, cw, cx, cy, cz, f_iters) - 
                           map_distance(px, py - normal_eps, pz, cw, cx, cy, cz, f_iters)
                local nz = map_distance(px, py, pz + normal_eps, cw, cx, cy, cz, f_iters) - 
                           map_distance(px, py, pz - normal_eps, cw, cx, cy, cz, f_iters)
                nx, ny, nz = normalize(nx, ny, nz)

                local ao = 1.0 - (t / 6.0)
                if ao < 0.0 then ao = 0.0 end
                
                local diff = nx*lx + ny*ly + nz*lz
                if diff < 0.0 then diff = 0.0 end
                
                local dot_rd_n = rd_x*nx + rd_y*ny + rd_z*nz
                local rx = rd_x - 2.0 * dot_rd_n * nx
                local ry = rd_y - 2.0 * dot_rd_n * ny
                local rz = rd_z - 2.0 * dot_rd_n * nz
                
                local spec = rx*lx + ry*ly + rz*lz
                if spec < 0.0 then spec = 0.0 end
                spec = spec ^ 24.0 
                
                final_r = (surf_r * diff * ao) + (spec * 0.8)
                final_g = (surf_g * diff * ao) + (spec * 0.8)
                final_b = (surf_b * diff * ao) + (spec * 0.8)
            end
            
            -- Add Nebula Glow
            final_r = final_r + glow_accum * glow_r * 0.8
            final_g = final_g + glow_accum * glow_g * 0.8
            final_b = final_b + glow_accum * glow_b * 0.8

            -- Prevent negative values before Tonemapping
            final_r = math.max(0.0, final_r)
            final_g = math.max(0.0, final_g)
            final_b = math.max(0.0, final_b)

            -- Tone Mapping (Reinhard)
            final_r = final_r / (1.0 + final_r)
            final_g = final_g / (1.0 + final_g)
            final_b = final_b / (1.0 + final_b)
            
            -- Gamma Correction (Linear -> sRGB)
            final_r = math.sqrt(final_r)
            final_g = math.sqrt(final_g)
            final_b = math.sqrt(final_b)
            
            -- =========================================================
            -- THE FIX: Industry Standard Spatial Dithering applied 
            -- *after* Gamma correction to prevent value blow-out.
            -- =========================================================
            local noise = ((math.sin(x * 12.9898 + y * 78.233) * 43758.5453) % 1.0 - 0.5) * 0.015
            final_r = final_r + noise
            final_g = final_g + noise
            final_b = final_b + noise
            
            -- Strict clamp & cast to 8-bit color
            local out_r = math.floor(math.max(0.0, math.min(1.0, final_r)) * 255)
            local out_g = math.floor(math.max(0.0, math.min(1.0, final_g)) * 255)
            local out_b = math.floor(math.max(0.0, math.min(1.0, final_b)) * 255)

            set_pixel(x, y, out_r, out_g, out_b)
        end
    end

    return true
end
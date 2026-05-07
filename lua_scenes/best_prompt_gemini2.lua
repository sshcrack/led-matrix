name = "mobius_fluid_manifold"

function setup()
    -- Mathematical parameters
    define_property("zoom", "float", 1.2, 0.1, 10.0)
    define_property("speed", "float", 0.5, 0.0, 5.0)
    define_property("iterations", "int", 5.0, 1.0, 10.0)
    
    -- Aesthetic properties
    define_property("tint_base", "color", 0x0A0F24) -- Deep abyss blue
    define_property("tint_peak", "color", 0xFF0055) -- Neon pink
    define_property("iridescence", "float", 1.2, 0.0, 3.0)
    define_property("light_height", "float", 1.5, 0.1, 5.0)
end

function initialize()
    log("Mobius Fluid Manifold initialized.")
    log("Canvas: " .. width .. "x" .. height .. " | Executing analytical PBR math...")
end

-- ============================================================================
-- CORE MATHEMATICS
-- ============================================================================

-- Iterated Function System (IFS) to generate organic heightfield manifold
local function evaluate_manifold(x, y, t, iters)
    local h = 0.0
    local amp = 1.0
    local freq = 1.0
    
    -- Precalculated rotation matrix coefficients for domain warping
    local rot_cos = math.cos(0.5)
    local rot_sin = math.sin(0.5)
    
    for i = 1, iters do
        -- Intersecting sine waves
        h = h + amp * math.abs(math.sin(x * freq + t) * math.cos(y * freq - t))
        
        -- Domain warp & rotate coordinates for the next octave
        local nx = x * rot_cos - y * rot_sin
        local ny = x * rot_sin + y * rot_cos
        
        -- Non-linear space offset
        x = nx + math.sin(y * freq + t) * 0.4
        y = ny + math.cos(x * freq - t) * 0.4
        
        -- Fractal scaling
        freq = freq * 1.9
        amp = amp * 0.55
    end
    return h
end

-- Fast color clamp helper
local function clamp_color(v)
    if v < 0.0 then return 0 end
    if v > 255.0 then return 255 end
    return math.floor(v)
end

-- ============================================================================
-- MAIN RENDER LOOP
-- ============================================================================

function render()
    -- 1. Cache properties outside the per-pixel loop for performance
    local zoom = get_property("zoom")
    local speed = get_property("speed")
    local iters = math.floor(get_property("iterations")) -- Ensure int is a proper integer
    local irid = get_property("iridescence")
    local lh = get_property("light_height")
    
    -- 2. Unpack 24-bit Colors
    local c_base = get_property("tint_base")
    local br = math.floor(c_base / 65536) % 256
    local bg = math.floor(c_base / 256) % 256
    local bb = c_base % 256

    local c_peak = get_property("tint_peak")
    local pr = math.floor(c_peak / 65536) % 256
    local pg = math.floor(c_peak / 256) % 256
    local pb = c_peak % 256

    -- 3. Calculate Global Scene State
    local t = time * speed
    local aspect = width / height

    -- Precompute complex coefficients for Möbius Transformation: f(z) = (az + b) / (cz + d)
    local ar, ai = math.cos(t * 0.3), math.sin(t * 0.4)
    local br_c, bi_c = 0.5 * math.sin(t * 0.7), 0.5 * math.cos(t * 0.2)
    local cr, ci = math.sin(t * 0.5), -math.cos(t * 0.6)
    local dr, di = 0.8 * math.cos(t * 0.1), 0.8 * math.sin(t * 0.8)

    -- Precompute light direction (Orbiting point light)
    local lx = math.sin(t * 0.5)
    local ly = math.cos(t * 0.5)
    local lz = lh
    local llen = math.sqrt(lx*lx + ly*ly + lz*lz)
    lx, ly, lz = lx/llen, ly/llen, lz/llen
    
    -- View vector (looking straight down into the screen)
    local vx, vy, vz = 0.0, 0.0, 1.0 

    -- Precalculate Half-way vector for Blinn-Phong Specular
    local h_x, h_y, h_z = lx + vx, ly + vy, lz + vz
    local h_len = math.sqrt(h_x*h_x + h_y*h_y + h_z*h_z)
    h_x, h_y, h_z = h_x/h_len, h_y/h_len, h_z/h_len

    -- 4. Per-Pixel Rendering
    for y = 0, height - 1 do
        -- Normalize Y to [-1, 1]
        local uv_y = (y / height - 0.5) * 2.0 * zoom
        
        for x = 0, width - 1 do
            -- Normalize X to [-1, 1] with Aspect Ratio correction
            local uv_x = (x / width - 0.5) * 2.0 * aspect * zoom
            
            -- --- A. COMPLEX SPACE WARPING (Möbius Transform) ---
            -- Complex multiplication and addition inlined for extreme performance
            -- num = a*z + b
            local num_r = ar*uv_x - ai*uv_y + br_c
            local num_i = ar*uv_y + ai*uv_x + bi_c
            -- den = c*z + d
            local den_r = cr*uv_x - ci*uv_y + dr
            local den_i = cr*uv_y + ci*uv_x + di
            
            -- Complex division: z' = num / den
            local den_mag = den_r*den_r + den_i*den_i + 1e-5 -- Prevent divide by zero
            local mx = (num_r*den_r + num_i*den_i) / den_mag
            local my = (num_i*den_r - num_r*den_i) / den_mag

            -- --- B. ANALYTICAL GRADIENTS ---
            -- We sample the heightmap at microscopic offsets to calculate normals
            local eps = 0.01
            local h  = evaluate_manifold(mx, my, t, iters)
            local hx = evaluate_manifold(mx + eps, my, t, iters)
            local hy = evaluate_manifold(mx, my + eps, t, iters)

            -- Finite difference to get slope
            local dx = (hx - h) / eps
            local dy = (hy - h) / eps
            local dz = 2.0 -- Controls surface bumpiness/steepness

            -- Normalize normal vector
            local len = math.sqrt(dx*dx + dy*dy + dz*dz)
            local nx, ny, nz = dx/len, dy/len, dz/len

            -- --- C. ILLUMINATION MODEL ---
            -- Diffuse lighting
            local ndotl = math.max(0.0, nx*lx + ny*ly + nz*lz)
            
            -- Specular highlight (Blinn-Phong)
            local ndoth = math.max(0.0, nx*h_x + ny*h_y + nz*h_z)
            local spec = ndoth ^ 64.0
            
            -- Fresnel Reflectance (Schlick's approximation)
            local ndotv = math.max(0.0, nx*vx + ny*vy + nz*vz)
            local fresnel = (1.0 - ndotv) ^ 3.0

            -- --- D. COLOR SYNTHESIS ---
            -- Blend primary/secondary colors based on elevation and grazing angle (fresnel)
            local mix = math.min(1.0, math.max(0.0, h * 0.6 + fresnel * irid))
            
            local out_r = br + (pr - br) * mix
            local out_g = bg + (pg - bg) * mix
            local out_b = bb + (pb - bb) * mix

            -- Phase shifting iridescence based on domain cross-product (creates rainbow oil-slick effect)
            local phase = mx * ny - my * nx + t
            out_r = out_r + math.sin(phase * 4.0) * 30.0 * irid
            out_g = out_g + math.sin(phase * 4.0 + 2.0) * 30.0 * irid
            out_b = out_b + math.sin(phase * 4.0 + 4.0) * 30.0 * irid

            -- Apply Lighting (Diffuse attenuation + Additive Specular highlight)
            out_r = out_r * (0.3 + 0.7 * ndotl) + spec * 255.0
            out_g = out_g * (0.3 + 0.7 * ndotl) + spec * 255.0
            out_b = out_b * (0.3 + 0.7 * ndotl) + spec * 255.0

            -- Commit to screen
            set_pixel(x, y, clamp_color(out_r), clamp_color(out_g), clamp_color(out_b))
        end
    end

    -- Required to keep the scene active
    return true
end
name = "hyperdimensional_manifold"
external_render_only = true

-- Cache math functions locally for maximum performance during the heavy raymarching loop
local sin, cos, sqrt, abs, max, min, floor, exp = math.sin, math.cos, math.sqrt, math.abs, math.max, math.min, math
.floor, math.exp

function setup()
    -- Expert-level property exposure for live manipulation
    define_property("time_scale", "float", 0.6, 0.1, 2.0)
    define_property("camera_zoom", "float", 1.2, 0.5, 3.0)
    define_property("glow_yield", "float", 1.8, 0.1, 5.0)
    define_property("max_steps", "int", 35, 10, 80)

    -- The core quantum emission frequency (base color)
    -- Default is a hyper-cyan/teal
    define_property("emission_color", "color", 0x00FFBB)
end

function initialize()
    log("Hyperdimensional Manifold Initialized.")
    log("Compiling Volumetric SDFs and Non-Euclidean Camera Matrices...")
end

-- Polynomial Smooth Maximum
-- Used to carve fluid, organic intersections between rigid geometric SDFs
local function smax(a, b, k)
    local h = max(k - abs(a - b), 0.0) / k
    return max(a, b) + h * h * k * 0.25
end

-- The Signed Distance Field (SDF) of the universe
-- Maps a 3D point (x,y,z) to the nearest distance of our mathematical geometry
local function map(x, y, z, t)
    -- 1. Infinite Domain Repetition along Z axis
    local spacing = 5.0
    local lz = (z + spacing * 0.5) % spacing - spacing * 0.5

    -- 2. Inverted Cylinder (Forms the base infinite tunnel bounding the universe)
    local tunnel_radius = 1.8 + sin(z * 0.5 + t) * 0.3
    local cyl = tunnel_radius - sqrt(x * x + y * y)

    -- 3. KIFS Folded Octahedrons
    -- We fold space across all 3 axis symmetrically
    local fx, fy, fz = abs(x), abs(y), abs(lz)
    -- Distance to an octahedron centered at the repeating origin
    local oct = (fx + fy + fz - 2.5) * 0.57735027

    -- 4. High-frequency Gyroid Manifold
    -- This adds the intricate, organic "alien webbing" displacement
    local scale = 3.0
    local g = (sin(x * scale) * cos(y * scale) + sin(y * scale) * cos(z * scale) + sin(z * scale) * cos(x * scale)) /
    scale

    -- 5. Constructive Solid Geometry (CSG)
    -- We elegantly carve the KIFS octahedrons OUT of the solid tunnel using smooth maximum,
    -- then displace the resulting manifold with our gyroid function.
    local hollow = smax(cyl, -oct, 1.2)

    -- Return final distance (scaled slightly to prevent ray-stepping artifacts across the Lipschitz boundary)
    return hollow - g * 0.4
end

function render()
    -- Map properties
    local t = time * get_property("time_scale")
    local zoom = get_property("camera_zoom")
    local glow_yield = get_property("glow_yield")
    local steps = floor(get_property("max_steps"))

    -- Unpack true 24-bit color into normalized RGB vectors [0.0 - 1.0]
    local raw_color = get_property("emission_color")
    local base_r = (floor(raw_color / 65536) % 256) / 255.0
    local base_g = (floor(raw_color / 256) % 256) / 255.0
    local base_b = (raw_color % 256) / 255.0

    local aspect = width / height

    -- ==========================================
    -- CAMERA KINEMATICS (Orthogonal Basis Matrix)
    -- ==========================================
    -- Dynamic swaying camera path flying through the domain
    local cam_x = sin(t * 0.6) * 0.8
    local cam_y = cos(t * 0.4) * 0.8
    local cam_z = t * 2.5

    -- The camera's "look at" target, slightly ahead on the curve
    local target_x = sin((t + 0.5) * 0.6) * 0.8
    local target_y = cos((t + 0.5) * 0.4) * 0.8
    local target_z = cam_z + 1.0

    -- Forward Vector (Z)
    local fw_x, fw_y, fw_z = target_x - cam_x, target_y - cam_y, target_z - cam_z
    local fw_len = sqrt(fw_x * fw_x + fw_y * fw_y + fw_z * fw_z)
    fw_x, fw_y, fw_z = fw_x / fw_len, fw_y / fw_len, fw_z / fw_len

    -- Dynamic Roll (Banking the camera as it turns)
    local roll = sin(t * 0.5) * 0.6
    local su, cu = sin(roll), cos(roll)
    local up_x, up_y, up_z = -su, cu, 0.0

    -- Right Vector (X) = Cross(Forward, Up)
    local r_x = up_y * fw_z - up_z * fw_y
    local r_y = up_z * fw_x - up_x * fw_z
    local r_z = up_x * fw_y - up_y * fw_x
    local r_len = sqrt(r_x * r_x + r_y * r_y + r_z * r_z)
    r_x, r_y, r_z = r_x / r_len, r_y / r_len, r_z / r_len

    -- True Up Vector (Y) = Cross(Right, Forward)
    local u_x = fw_y * r_z - fw_z * r_y
    local u_y = fw_z * r_x - fw_x * r_z
    local u_z = fw_x * r_y - fw_y * r_x

    -- ==========================================
    -- RAYMARCHING ENGINE
    -- ==========================================
    for py = 0, height - 1 do
        for px = 0, width - 1 do
            -- Transform pixel into Normalized Device Coordinates [-1.0 to 1.0]
            local uv_x = (px / width * 2.0 - 1.0) * aspect
            local uv_y = (py / height * 2.0 - 1.0)

            -- Construct ray direction by multiplying UVs by the Camera Basis Matrix
            local rd_x = uv_x * r_x + uv_y * u_x + fw_x * zoom
            local rd_y = uv_x * r_y + uv_y * u_y + fw_y * zoom
            local rd_z = uv_x * r_z + uv_y * u_z + fw_z * zoom

            -- Normalize Ray Direction
            local rd_len = sqrt(rd_x * rd_x + rd_y * rd_y + rd_z * rd_z)
            rd_x, rd_y, rd_z = rd_x / rd_len, rd_y / rd_len, rd_z / rd_len

            -- Raymarching states
            local total_dist = 0.0
            local accum_glow_r = 0.0
            local accum_glow_g = 0.0
            local accum_glow_b = 0.0
            local hit = false

            -- Step the ray through the vector field
            for i = 1, steps do
                local p_x = cam_x + rd_x * total_dist
                local p_y = cam_y + rd_y * total_dist
                local p_z = cam_z + rd_z * total_dist

                -- Sample the universe's distance field
                local dist = map(p_x, p_y, p_z, t)

                -- ==========================================
                -- VOLUMETRIC INTEGRATION
                -- ==========================================
                -- As rays graze surfaces, they accumulate energy.
                -- We use an exponential falloff based on the distance to create a pseudo-scattered plasma glow.
                -- We offset the color channels spatially to simulate chromatic aberration / iridescence.
                local density = exp(-dist * 4.5)
                accum_glow_r = accum_glow_r + density * (0.6 + 0.4 * sin(p_z * 1.0 + t)) * base_r * 0.06
                accum_glow_g = accum_glow_g + density * (0.6 + 0.4 * sin(p_z * 1.1 + t)) * base_g * 0.06
                accum_glow_b = accum_glow_b + density * (0.6 + 0.4 * sin(p_z * 1.2 + t)) * base_b * 0.06

                -- Check for solid hit or infinity escape
                if dist < 0.002 then
                    hit = true
                    break
                end
                if total_dist > 15.0 then
                    break
                end

                -- Step forward (multiplied by a safety factor to prevent tunneling artifacts)
                total_dist = total_dist + dist * 0.6
            end

            -- Combine volumetric scatter with base colors
            local final_r = accum_glow_r * glow_yield
            local final_g = accum_glow_g * glow_yield
            local final_b = accum_glow_b * glow_yield

            -- Add fake ambient occlusion based on ray distance if we hit geometry
            if hit then
                local ao = 1.0 / (1.0 + total_dist * total_dist * 0.05)
                final_r = final_r + ao * base_r * 0.2
                final_g = final_g + ao * base_g * 0.2
                final_b = final_b + ao * base_b * 0.2
            end

            -- ==========================================
            -- POST-PROCESSING (Reinhard Tone Mapping & Gamma)
            -- ==========================================
            -- Smoothly map infinitely bright HDR values back into a 0.0-1.0 range
            final_r = final_r / (final_r + 1.0)
            final_g = final_g / (final_g + 1.0)
            final_b = final_b / (final_b + 1.0)

            -- Gamma Correction approximation (1.0/2.2 ~ 0.5)
            final_r = sqrt(final_r)
            final_g = sqrt(final_g)
            final_b = sqrt(final_b)

            -- Quantize to 8-bit color space and protect against bounds
            local r_out = floor(max(0, min(255, final_r * 255)))
            local g_out = floor(max(0, min(255, final_g * 255)))
            local b_out = floor(max(0, min(255, final_b * 255)))

            set_pixel(px, py, r_out, g_out, b_out)
        end
    end

    return true
end

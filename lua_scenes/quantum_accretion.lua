-- ============================================================================
-- File: quantum_accretion.lua
-- Concept: Domain-Warped Fractal Brownian Motion (FBM) with Differential Rotation
-- ============================================================================

name = "quantum_accretion"

-- ============================================================================
-- LUA PERFORMANCE OPTIMIZATION
-- Caching global math functions to local variables speeds up execution by ~30%
-- inside the deep nested render loop.
-- ============================================================================
local m_sin   = math.sin
local m_cos   = math.cos
local m_floor = math.floor
local m_abs   = math.abs
local m_sqrt  = math.sqrt
local m_max   = math.max
local m_min   = math.min
local m_atan2 = math.atan2

-- ============================================================================
-- SETUP & INITIALIZATION
-- ============================================================================
function setup()
    -- Visual controllers
    define_property("speed", "float", 0.5, 0.0, 3.0)
    define_property("zoom", "float", 2.5, 0.5, 10.0)
    define_property("intensity", "float", 1.2, 0.1, 5.0)
    
    -- Int property: Controls the fractal octaves (higher = more detail, lower FPS)
    define_property("detail_level", "int", 3, 1, 5)
    
    -- Colors: Core (Fiery Orange/White) and Nebula (Deep Space Blue/Purple)
    define_property("core_color", "color", 0xFF8822) 
    define_property("nebula_color", "color", 0x220088)
end

function initialize()
    log("Quantum Accretion Initialized: Engaging FBM engine at " .. width .. "x" .. height)
end

-- ============================================================================
-- PROCEDURAL NOISE ENGINE (Pure Math)
-- ============================================================================

-- Fast fractional part
local function fract(x) 
    return x - m_floor(x) 
end

-- 2D Hash function for pseudo-random gradient mapping
local function hash(x, y)
    local a = x * 12.9898 + y * 78.233
    return fract(m_sin(a) * 43758.5453123)
end

-- Smooth 2D Value Noise
local function noise(x, y)
    local ix, iy = m_floor(x), m_floor(y)
    local fx, fy = fract(x), fract(y)
    
    -- Smoothstep interpolation
    local ux = fx * fx * (3.0 - 2.0 * fx)
    local uy = fy * fy * (3.0 - 2.0 * fy)
    
    local a = hash(ix, iy)
    local b = hash(ix + 1.0, iy)
    local c = hash(ix, iy + 1.0)
    local d = hash(ix + 1.0, iy + 1.0)
    
    return a + (b - a)*ux + (c - a)*uy + (a - b - c + d)*ux*uy
end

-- "Ridge" Fractal Brownian Motion: Creates lightning-like plasma tendrils
local function ridge_fbm(x, y, octaves)
    local v = 0.0
    local amplitude = 0.5
    local frequency = 1.0
    
    -- Rotation matrix to break up grid artifacts
    local cos2, sin2 = 0.866, 0.5 
    
    for i = 1, octaves do
        -- Fold the noise to create sharp ridges
        local n = m_abs(noise(x * frequency, y * frequency) * 2.0 - 1.0)
        v = v + (1.0 - n) * amplitude
        
        -- Rotate and scale coordinates for next octave
        local nx = x * cos2 - y * sin2
        local ny = x * sin2 + y * cos2
        x, y = nx * 2.0, ny * 2.0
        
        amplitude = amplitude * 0.5
    end
    return v
end

-- Smoothstep helper for soft edges/event horizons
local function smoothstep(edge0, edge1, x)
    local t = m_max(0.0, m_min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)
end

-- ============================================================================
-- MAIN RENDER LOOP
-- ============================================================================
function render()
    clear()

    -- 1. Extract and Cast Properties
    local speed = get_property("speed")
    local zoom = get_property("zoom")
    local intensity = get_property("intensity")
    local detail = m_floor(get_property("detail_level")) -- Coerce to int

    -- Unpack Core Color (24-bit Hex)
    local hex_core = get_property("core_color")
    local cr = m_floor(hex_core / 65536) % 256
    local cg = m_floor(hex_core / 256) % 256
    local cb = hex_core % 256

    -- Unpack Nebula Color (24-bit Hex)
    local hex_neb = get_property("nebula_color")
    local nr = m_floor(hex_neb / 65536) % 256
    local ng = m_floor(hex_neb / 256) % 256
    local nb = hex_neb % 256

    local t = time * speed

    -- Aspect ratio adjustment
    local aspect = width / height
    local half_w = width * 0.5
    local half_h = height * 0.5

    -- 2. Pixel Iteration Loop
    for px = 0, width - 1 do
        for py = 0, height - 1 do
            
            -- Normalize UV coordinates (-1.0 to 1.0)
            local uv_x = (px - half_w) / half_h * aspect
            local uv_y = (py - half_h) / half_h
            
            -- Distance and angle from center
            local dist = m_sqrt(uv_x*uv_x + uv_y*uv_y)
            
            -- Differential Rotation: Closer to center spins faster
            local angle = t + (1.0 / (dist + 0.2))
            local s, c = m_sin(angle), m_cos(angle)
            
            -- Rotate and apply zoom
            local rx = (uv_x * c - uv_y * s) * zoom
            local ry = (uv_x * s + uv_y * c) * zoom
            
            -- Domain Warping (Distorting space with noise before calculating noise)
            local warp_x = ridge_fbm(rx + t * 0.2, ry - t * 0.1, 2)
            local warp_y = ridge_fbm(rx - t * 0.3, ry + t * 0.2, 2)
            
            -- Calculate the final Plasma density
            local plasma = ridge_fbm(rx + warp_x * 2.0, ry + warp_y * 2.0, detail)
            
            -- Apply "Event Horizon" mask (dark in the very center, fading out)
            local horizon = smoothstep(0.1, 0.5, dist)
            plasma = plasma * horizon
            
            -- Core Glow (Inverse square falloff from center)
            local glow = (0.03 * intensity) / (dist + 0.001)
            
            -- 3. Thermodynamic Color Compositing
            -- We transition: Dark Space -> Nebula Gas -> Searing Core
            local mix_factor = m_max(0.0, m_min(1.0, plasma * intensity))
            
            local final_r, final_g, final_b = 0, 0, 0
            
            if mix_factor < 0.5 then
                -- Blend Dark to Nebula
                local st = mix_factor * 2.0
                final_r = nr * st
                final_g = ng * st
                final_b = nb * st
            else
                -- Blend Nebula to Core
                local st = (mix_factor - 0.5) * 2.0
                final_r = nr + (cr - nr) * st
                final_g = ng + (cg - ng) * st
                final_b = nb + (cb - nb) * st
            end
            
            -- Add pure physical core glow on top (additive blending)
            final_r = final_r + (cr * glow)
            final_g = final_g + (cg * glow)
            final_b = final_b + (cb * glow)
            
            -- Clamp safe output (0-255)
            final_r = m_max(0, m_min(255, m_floor(final_r)))
            final_g = m_max(0, m_min(255, m_floor(final_g)))
            final_b = m_max(0, m_min(255, m_floor(final_b)))
            
            -- Draw to screen
            set_pixel(px, py, final_r, final_g, final_b)
        end
    end

    return true
end
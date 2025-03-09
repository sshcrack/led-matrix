//CODE BY https://www.shadertoy.com/view/Dsc3R4


#include "FireScene.h"
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace AmbientScenes;

FireScene::FireScene() : Scene() {
    accumulated_time = 0;
    time = 0.0f;
    
    // Initialize RNG with a time-based seed
    rng = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
}

void FireScene::initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    width = matrix->width();
    height = matrix->height();
    last_update = std::chrono::steady_clock::now();
    
    // Set up distributions for spark generation
    dist_x = std::uniform_real_distribution<float>(0.0f, static_cast<float>(width));
    dist_vel = std::uniform_real_distribution<float>(0.5f, 2.0f);
    dist_life = std::uniform_real_distribution<float>(0.5f, 2.0f);
    
    // Pre-allocate space for maximum expected number of sparks
    sparks.reserve(50);
}

float FireScene::hash12(const float x, const float y) {
    // Better hash function with improved randomness
    float a = std::sin(x * 12.9898f + y * 78.233f) * 43758.5453f;
    float b = std::sin(y * 12.9898f + x * 78.233f) * 39725.1846f;
    return std::fmod(a * b, 1.0f);
}

float FireScene::linear_noise(const float x, const float y) {
    int ix = std::floor(x);
    int iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;

    // Improved smoothstep interpolation
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float v1 = hash12(ix, iy);
    float v2 = hash12(ix + 1, iy);
    float v3 = hash12(ix, iy + 1);
    float v4 = hash12(ix + 1, iy + 1);

    return std::lerp(
            std::lerp(v1, v2, fx),
            std::lerp(v3, v4, fx),
            fy
    );
}

float FireScene::fbm(const float x, const float y) const {
    // Apply wind effect
    float wind_offset = wind_strength->get() * time * 0.5f;
    float uv_x = x * 3.0f - 1.0f + wind_offset;
    
    // Adjust the speed of fire movement
    float speed_factor = fire_speed->get();
    float uv_y = y * 2.0f - 0.25f - 2.0f * time * speed_factor;

    float f = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;

    // Use more octaves for better detail
    for (int i = 0; i < 4; ++i) {
        f += amp * linear_noise(uv_x * freq + time * 0.5f * speed_factor, 
                               uv_y * freq + time * 0.7f * speed_factor);
        amp *= 0.5f;
        freq *= 2.0f;
    }
    
    // Scale by intensity
    return f * 0.5f * fire_intensity->get();
}

void FireScene::maybe_emit_spark() {
    // Limit the maximum number of sparks
    if (sparks.size() >= 20 || !show_sparks->get()) {
        return;
    }
    
    // Random chance to emit a spark
    if (hash12(time, time * 0.37f) < 0.1f * fire_intensity->get()) {
        float x = dist_x(rng);
        float y = static_cast<float>(height - 1);  // Start at bottom
        float velocity = dist_vel(rng) * fire_intensity->get();
        float lifespan = dist_life(rng);
        
        sparks.emplace_back(x, y, velocity, lifespan);
    }
}

void FireScene::update_sparks(float delta_time) {
    if (!show_sparks->get()) {
        sparks.clear();
        return;
    }
    
    for (auto it = sparks.begin(); it != sparks.end();) {
        auto& [x, y, vel, life] = *it;
        
        // Update position and life
        y -= vel * delta_time * 30.0f;
        x += wind_strength->get() * vel * delta_time * 15.0f;
        life -= delta_time;
        
        // Remove if off-screen or dead
        if (y < 0 || x < 0 || x >= width || life <= 0) {
            it = sparks.erase(it);
        } else {
            ++it;
        }
    }
}

void FireScene::render_sparks(rgb_matrix::Canvas *canvas) {
    if (!show_sparks->get()) {
        return;
    }
    
    for (const auto& [x, y, vel, life] : sparks) {
        int ix = static_cast<int>(x);
        int iy = static_cast<int>(y);
        
        if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
            // Spark brightness based on remaining life
            float brightness = std::min(life * 0.8f, 1.0f);
            
            // Yellow-white sparks with some flicker
            float flicker = hash12(time + x, time + y) * 0.3f + 0.7f;
            int r = std::min(255, static_cast<int>(255 * brightness * flicker));
            int g = std::min(255, static_cast<int>(220 * brightness * flicker));
            int b = std::min(255, static_cast<int>(160 * brightness * flicker));
            
            canvas->SetPixel(ix, iy, r, g, b);
        }
    }
}

void FireScene::render_fire(rgb_matrix::Canvas *canvas) {
    for (int y = 0; y < height; y++) {
        float uv_y = static_cast<float>(y) / height;
        for (int x = 0; x < width; x++) {
            float uv_x = static_cast<float>(x) / width;

            float fbm_val = fbm(uv_x, uv_y);
            float intensity = std::clamp(fbm_val * 1.7f, 0.0f, 1.0f);

            // Enhanced vertical gradient for more realistic flames
            float y_factor = std::pow(1.0f - uv_y, 1.5f);
            intensity *= y_factor;
            
            int r, g, b;
            
            // Different color schemes
            switch (color_scheme->get()) {
                case 1: // Blue fire
                    r = std::min(255, static_cast<int>(intensity * intensity * 180.0f));
                    g = std::min(255, static_cast<int>(intensity * intensity * 220.0f));
                    b = std::min(255, static_cast<int>(intensity * 255.0f));
                    break;
                    
                case 2: // Green fire
                    r = std::min(255, static_cast<int>(intensity * intensity * 180.0f));
                    g = std::min(255, static_cast<int>(intensity * 255.0f));
                    b = std::min(255, static_cast<int>(intensity * intensity * 160.0f));
                    break;
                    
                case 3: // Purple fire
                    r = std::min(255, static_cast<int>(intensity * 220.0f));
                    g = std::min(255, static_cast<int>(intensity * intensity * 130.0f));
                    b = std::min(255, static_cast<int>(intensity * 255.0f));
                    break;
                    
                default: // Regular orange-yellow fire (default)
                    // More vibrant orange gradient
                    r = std::min(255, static_cast<int>(intensity * 255.0f));
                    g = std::min(255, static_cast<int>(intensity * std::sqrt(intensity) * 200.0f));
                    b = std::min(255, static_cast<int>(intensity * intensity * intensity * 160.0f));
                    break;
            }

            canvas->SetPixel(x, y, r, g, b);
        }
    }
}

bool FireScene::render(RGBMatrixBase *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;

    accumulated_time += delta_time;
    if (accumulated_time < update_delay) {
        return true;
    }
    accumulated_time -= update_delay;

    time += delta_time;  // Update simulation time
    
    // Clear canvas before rendering
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            offscreen_canvas->SetPixel(x, y, 0, 0, 0);
        }
    }
    
    // Render the fire effect
    render_fire(offscreen_canvas);
    
    // Handle sparks
    maybe_emit_spark();
    update_sparks(delta_time);
    render_sparks(offscreen_canvas);

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string FireScene::get_name() const {
    return "fire";
}

void FireScene::register_properties() {
    add_property(frames_per_second);
    add_property(fire_intensity);
    add_property(fire_speed);
    add_property(wind_strength);
    add_property(show_sparks);
    add_property(color_scheme);
}

void FireScene::load_properties(const json &j) {
    Scene::load_properties(j);

    update_delay = 1.0f / frames_per_second->get();
}

void deleteFireScene(Scenes::Scene *scene) {
    delete scene;
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> FireSceneWrapper::create() {
    return {new FireScene(), [](Scenes::Scene *scene) {
        delete dynamic_cast<FireScene *>(scene);
    }};
}
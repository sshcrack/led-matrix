//CODE BY https://www.shadertoy.com/view/Dsc3R4


#include "FireScene.h"
#include <algorithm>
#include <cmath>

using namespace Scenes;

FireScene::FireScene(const nlohmann::json &config) : Scene(config) {
    update_delay = 1.0f / config.value("fps", 30.0f);
    accumulated_time = 0;
    time = 0.0f;
}

void FireScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    width = matrix->width();
    height = matrix->height();
    last_update = std::chrono::steady_clock::now();
}

float FireScene::hash12(float x, float y) {
    // Simplified hash function
    return std::fmod(std::sin(x * 12.9898f + y * 78.233f) * 43758.5453f, 1.0f);
}

float FireScene::linear_noise(float x, float y) const {
    int ix = std::floor(x);
    int iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;
    
    // Simpler interpolation
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

float FireScene::fbm(float x, float y) const {
    float uv_x = x * 3.0f - 1.0f;
    float uv_y = y * 2.0f - 0.25f - 2.0f * time;
    
    float f = 1.0f;
    float amp = 0.5f;
    
    // Reduced number of iterations from 5 to 3
    for(int i = 0; i < 3; ++i) {
        f += amp * linear_noise(uv_x + time * 0.5f, uv_y + time * 0.7f);
        amp *= 0.5f;
        uv_x *= 2.0f;
        uv_y *= 2.0f;
    }
    return f * 0.5f;
}

void FireScene::render_fire(rgb_matrix::Canvas* canvas) {
    for (int y = 0; y < height; y++) {
        float uv_y = static_cast<float>(y) / height;
        for (int x = 0; x < width; x++) {
            float uv_x = static_cast<float>(x) / width;
            
            float fbm_val = fbm(uv_x, uv_y);
            float intensity = std::clamp(fbm_val * 1.5f, 0.0f, 1.0f);
            
            // Simplified color calculation
            float y_factor = 1.0f - uv_y;
            intensity *= y_factor * y_factor;
            
            // Simplified color mapping
            int r = std::min(255, static_cast<int>(intensity * 255.0f));
            int g = std::min(255, static_cast<int>(intensity * intensity * 200.0f));
            int b = std::min(255, static_cast<int>(intensity * intensity * intensity * 160.0f));
            
            canvas->SetPixel(x, y, r, g, b);
        }
    }
}

bool FireScene::render(rgb_matrix::RGBMatrix *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    
    accumulated_time += delta_time;
    if (accumulated_time < update_delay) {
        return true;
    }
    accumulated_time -= update_delay;
    
    time += delta_time;  // Update simulation time
    render_fire(offscreen_canvas);
    
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string FireScene::get_name() const {
    return "fire";
}

Scene *FireSceneWrapper::create_default() {
    const nlohmann::json json = {
        {"weight", 1},
        {"duration", 15000},
        {"fps", 30},
        {"cooling_factor", 70},
        {"sparking_chance", 120},
        {"spark_height", 3}
    };
    return new FireScene(json);
}

Scene *FireSceneWrapper::from_json(const nlohmann::json &args) {
    return new FireScene(args);
}

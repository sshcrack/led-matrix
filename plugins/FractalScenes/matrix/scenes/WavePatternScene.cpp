#include "WavePatternScene.h"
#include <cmath>
#include <random>

using namespace Scenes;

WavePatternScene::WavePatternScene() : Scene() {
}

void WavePatternScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    last_update = std::chrono::steady_clock::now();
    total_time = 0.0f;
    init_waves();
}

void WavePatternScene::init_waves() {
    waves.clear();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> amp_dist(0.1f, 0.9f);
    std::uniform_real_distribution<float> freq_dist(0.5f, 4.0f);
    std::uniform_real_distribution<float> speed_dist(0.5f, 2.0f);
    std::uniform_real_distribution<float> phase_dist(0.0f, 6.28f);
    
    for (int i = 0; i < num_waves->get(); i++) {
        Wave wave;
        wave.amplitude = amp_dist(gen) * wave_height->get();
        wave.frequency = freq_dist(gen);
        wave.phase_speed = speed_dist(gen);
        wave.phase_offset = phase_dist(gen);
        waves.push_back(wave);
    }
}

bool WavePatternScene::render(RGBMatrixBase *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    total_time += delta_time * speed->get();
    
    int width = matrix->width();
    int height = matrix->height();
    
    // Clear the canvas
    offscreen_canvas->Clear();
    
    // Calculate wave values for each column
    for (int x = 0; x < width; ++x) {
        float normalized_x = static_cast<float>(x) / width;
        
        // Combine multiple waves
        float wave_value = 0.0f;
        for (const auto& wave : waves) {
            wave_value += wave.amplitude * std::sin(normalized_x * wave.frequency * 6.28f + 
                                                  total_time * wave.phase_speed + wave.phase_offset);
        }
        
        // Scale to fit the height
        float center_y = height / 2.0f;
        int y_pos = static_cast<int>(center_y + wave_value * center_y * 0.8f);
        
        // Clamp the position to the matrix boundaries
        y_pos = std::max(0, std::min(height - 1, y_pos));
        
        // Calculate color based on position and time
        uint8_t r, g, b;
        if (rainbow_mode->get()) {
            float hue = std::fmod(normalized_x + total_time * color_speed->get() * 0.1f, 1.0f);
            hsv_to_rgb(hue, 1.0f, 1.0f, r, g, b);
        } else {
            // Blue-cyan-white theme
            float intensity = (1.0f + wave_value) * 0.5f;
            b = static_cast<uint8_t>(255);
            g = static_cast<uint8_t>(100 + 155 * intensity);
            r = static_cast<uint8_t>(50 + 205 * intensity * intensity);
        }
        
        // Draw the wave pixel
        offscreen_canvas->SetPixel(x, y_pos, r, g, b);
        
        // Add glow effect
        for (int glow = 1; glow <= 2; glow++) {
            int glow_y_up = y_pos - glow;
            int glow_y_down = y_pos + glow;
            
            uint8_t glow_factor = static_cast<uint8_t>(255 / (glow + 1));
            
            if (glow_y_up >= 0) {
                offscreen_canvas->SetPixel(x, glow_y_up, r * glow_factor / 255, g * glow_factor / 255, b * glow_factor / 255);
            }
            if (glow_y_down < height) {
                offscreen_canvas->SetPixel(x, glow_y_down, r * glow_factor / 255, g * glow_factor / 255, b * glow_factor / 255);
            }
        }
    }
    
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string WavePatternScene::get_name() const {
    return "wave_pattern";
}

void WavePatternScene::register_properties() {
    add_property(num_waves);
    add_property(speed);
    add_property(color_speed);
    add_property(rainbow_mode);
    add_property(wave_height);
}

void WavePatternScene::load_properties(const json &j) {
    Scene::load_properties(j);
    init_waves();
}

// Convert HSV to RGB color
void WavePatternScene::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) const {
    float c = v * s;
    float x = c * (1 - std::abs(std::fmod(h * 6, 2) - 1));
    float m = v - c;
    float r1, g1, b1;
    
    if (h < 1.0f/6.0f) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 2.0f/6.0f) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 3.0f/6.0f) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 4.0f/6.0f) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 5.0f/6.0f) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    
    r = static_cast<uint8_t>((r1 + m) * 255);
    g = static_cast<uint8_t>((g1 + m) * 255);
    b = static_cast<uint8_t>((b1 + m) * 255);
}

std::unique_ptr<Scene, void (*)(Scene *)> WavePatternSceneWrapper::create() {
    return {
        new WavePatternScene(), [](Scene *scene) {
            delete dynamic_cast<WavePatternScene*>(scene);
        }
    };
}

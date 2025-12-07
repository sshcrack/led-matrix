#include "FireplaceScene.h"
#include <cmath>
#include <algorithm>

namespace Scenes {

FireplaceScene::FireplaceScene() 
    : rng(std::random_device{}()),
      dist_spawn_x(30.0f, 98.0f),
      dist_vy(20.0f, 50.0f),
      dist_vx(-5.0f, 5.0f),
      dist_life(0.5f, 2.0f),
      dist_intensity(0.5f, 1.0f),
      dist_heat(0.6f, 1.0f) {
}

void FireplaceScene::spawnFlame() {
    Flame flame;
    flame.x = dist_spawn_x(rng);
    flame.y = matrix_height - 25; // Spawn near the logs
    flame.vy = dist_vy(rng);
    flame.vx = dist_vx(rng);
    flame.life = 1.0f;
    flame.max_life = dist_life(rng);
    flame.intensity = dist_intensity(rng);
    flame.heat = dist_heat(rng);
    flames.push_back(flame);
}

void FireplaceScene::setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
        // Additive blending for fire glow
        uint8_t curr_r, curr_g, curr_b;
        offscreen_canvas->GetPixel(x, y, &curr_r, &curr_g, &curr_b);
        
        r = std::min(255, static_cast<int>(curr_r) + r);
        g = std::min(255, static_cast<int>(curr_g) + g);
        b = std::min(255, static_cast<int>(curr_b) + b);
        
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

rgb_matrix::Color FireplaceScene::getFlameColor(float heat, float intensity) {
    // Heat determines color progression: red -> orange -> yellow -> white
    uint8_t r, g, b;
    
    if (heat < 0.3f) {
        // Deep red to red
        r = static_cast<uint8_t>(150 + 105 * (heat / 0.3f));
        g = 0;
        b = 0;
    } else if (heat < 0.6f) {
        // Red to orange
        float t = (heat - 0.3f) / 0.3f;
        r = 255;
        g = static_cast<uint8_t>(165 * t);
        b = 0;
    } else if (heat < 0.85f) {
        // Orange to yellow
        float t = (heat - 0.6f) / 0.25f;
        r = 255;
        g = static_cast<uint8_t>(165 + 90 * t);
        b = static_cast<uint8_t>(30 * t);
    } else {
        // Yellow to white (hottest)
        float t = (heat - 0.85f) / 0.15f;
        r = 255;
        g = 255;
        b = static_cast<uint8_t>(30 + 225 * t);
    }
    
    // Apply intensity
    r = static_cast<uint8_t>(r * intensity);
    g = static_cast<uint8_t>(g * intensity);
    b = static_cast<uint8_t>(b * intensity);
    
    return rgb_matrix::Color(r, g, b);
}

void FireplaceScene::drawLogs() {
    // Log color (dark brown)
    uint8_t log_r = 60, log_g = 40, log_b = 20;
    uint8_t dark_r = 40, dark_g = 25, dark_b = 10;
    
    // Draw three logs at the bottom
    int log_y = matrix_height - 20;
    
    // Left log
    for (int y = log_y; y < log_y + 15; y++) {
        for (int x = 20; x < 50; x++) {
            bool is_edge = (y == log_y || y == log_y + 14 || x == 20 || x == 49);
            if (is_edge) {
                offscreen_canvas->SetPixel(x, y, dark_r, dark_g, dark_b);
            } else {
                offscreen_canvas->SetPixel(x, y, log_r, log_g, log_b);
            }
        }
    }
    
    // Center log (slightly higher)
    for (int y = log_y - 5; y < log_y + 10; y++) {
        for (int x = 45; x < 83; x++) {
            bool is_edge = (y == log_y - 5 || y == log_y + 9 || x == 45 || x == 82);
            if (is_edge) {
                offscreen_canvas->SetPixel(x, y, dark_r, dark_g, dark_b);
            } else {
                offscreen_canvas->SetPixel(x, y, log_r, log_g, log_b);
            }
        }
    }
    
    // Right log
    for (int y = log_y; y < log_y + 15; y++) {
        for (int x = 78; x < 108; x++) {
            bool is_edge = (y == log_y || y == log_y + 14 || x == 78 || x == 107);
            if (is_edge) {
                offscreen_canvas->SetPixel(x, y, dark_r, dark_g, dark_b);
            } else {
                offscreen_canvas->SetPixel(x, y, log_r, log_g, log_b);
            }
        }
    }
}

void FireplaceScene::drawEmbers(float t) {
    // Draw glowing embers on the logs
    std::mt19937 ember_rng(12345); // Fixed seed for consistent ember positions
    std::uniform_int_distribution<int> x_dist(25, 103);
    std::uniform_int_distribution<int> y_dist(matrix_height - 18, matrix_height - 8);
    std::uniform_real_distribution<float> phase_dist(0.0f, 6.28f);
    
    for (int i = 0; i < 15; i++) {
        int x = x_dist(ember_rng);
        int y = y_dist(ember_rng);
        float phase = phase_dist(ember_rng);
        
        // Pulsing glow
        float glow = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(t * 2.0f + phase));
        
        uint8_t r = static_cast<uint8_t>(255 * glow);
        uint8_t g = static_cast<uint8_t>(80 * glow);
        uint8_t b = 0;
        
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

bool FireplaceScene::render(RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    float dt = frame.dt;
    float t = frame.t;
    
    // Clear with background color
    auto bg = bg_color->get();
    offscreen_canvas->Fill(bg.r, bg.g, bg.b);
    
    // Draw logs
    if (show_logs->get()) {
        drawLogs();
    }
    
    // Spawn new flames
    int target_flames = flame_density->get();
    float spawn_rate = target_flames * 0.5f; // Flames per second
    int flames_to_spawn = static_cast<int>(spawn_rate * dt);
    
    for (int i = 0; i < flames_to_spawn; i++) {
        if (static_cast<int>(flames.size()) < target_flames) {
            spawnFlame();
        }
    }
    
    // Update and draw flames
    float speed_mult = flicker_speed->get();
    float heat_mult = heat_intensity->get();
    
    for (auto it = flames.begin(); it != flames.end();) {
        auto& flame = *it;
        
        // Update position
        flame.y -= flame.vy * dt * speed_mult;
        flame.x += flame.vx * dt;
        
        // Update life
        flame.life -= dt / flame.max_life;
        
        // Add turbulence
        flame.vx += (dist_vx(rng) - flame.vx) * dt * 2.0f;
        
        // Heat decreases as flame rises
        float heat = flame.heat * heat_mult * flame.life;
        float intensity = flame.intensity * flame.life;
        
        // Remove dead flames
        if (flame.life <= 0.0f || flame.y < 0) {
            it = flames.erase(it);
            continue;
        }
        
        // Draw flame
        int ix = static_cast<int>(flame.x);
        int iy = static_cast<int>(flame.y);
        
        auto color = getFlameColor(heat, intensity);
        
        // Main flame particle
        setPixelSafe(ix, iy, color.r, color.g, color.b);
        
        // Add glow/blur effect for larger flames
        if (intensity > 0.5f) {
            uint8_t glow_r = color.r * 0.5f;
            uint8_t glow_g = color.g * 0.5f;
            uint8_t glow_b = color.b * 0.5f;
            
            setPixelSafe(ix - 1, iy, glow_r, glow_g, glow_b);
            setPixelSafe(ix + 1, iy, glow_r, glow_g, glow_b);
            setPixelSafe(ix, iy - 1, glow_r, glow_g, glow_b);
            setPixelSafe(ix, iy + 1, glow_r, glow_g, glow_b);
        }
        
        ++it;
    }
    
    // Draw glowing embers
    if (show_embers->get()) {
        drawEmbers(t);
    }
    
    return true;
}

void FireplaceScene::register_properties() {
    add_property(flame_density);
    add_property(flicker_speed);
    add_property(heat_intensity);
    add_property(show_logs);
    add_property(show_embers);
    add_property(bg_color);
}

std::unique_ptr<Scene, void (*)(Scene *)> FireplaceSceneWrapper::create() {
    return {new FireplaceScene(), [](Scene *scene) { delete scene; }};
}

}

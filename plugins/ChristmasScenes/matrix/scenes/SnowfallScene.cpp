#include "SnowfallScene.h"
#include <cmath>
#include <algorithm>

namespace Scenes {

SnowfallScene::SnowfallScene() 
    : rng(std::random_device{}()),
      dist_x(0.0f, 127.0f),
      dist_y(-10.0f, 0.0f),
      dist_vy(10.0f, 30.0f),
      dist_size(0.5f, 2.0f),
      dist_brightness(200.0f, 255.0f),
      dist_phase(0.0f, 6.28f) {
    ground_height.resize(128, 0);
}

void SnowfallScene::initializeSnowflakes() {
    snowflakes.clear();
    int count = density->get();
    
    for (int i = 0; i < count; i++) {
        Snowflake flake;
        flake.x = dist_x(rng);
        flake.y = dist_y(rng);
        flake.vy = dist_vy(rng);
        flake.vx = 0.0f;
        flake.size = dist_size(rng);
        flake.phase = dist_phase(rng);
        flake.brightness = static_cast<uint8_t>(dist_brightness(rng));
        snowflakes.push_back(flake);
    }
}

void SnowfallScene::resetSnowflake(Snowflake& flake) {
    flake.x = dist_x(rng);
    flake.y = -2.0f;
    flake.vy = dist_vy(rng);
    flake.size = dist_size(rng);
    flake.phase = dist_phase(rng);
    flake.brightness = static_cast<uint8_t>(dist_brightness(rng));
}

void SnowfallScene::setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

bool SnowfallScene::render(RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    float dt = frame.dt;
    float t = frame.t;
    
    // Initialize on first frame
    if (snowflakes.empty() || static_cast<int>(snowflakes.size()) != density->get()) {
        initializeSnowflakes();
    }
    
    // Clear with background color
    auto bg = bg_color->get();
    offscreen_canvas->Fill(bg.r, bg.g, bg.b);
    
    // Draw accumulated snow
    if (accumulate->get()) {
        for (int x = 0; x < matrix_width; x++) {
            for (int y = 0; y < ground_height[x]; y++) {
                int screen_y = matrix_height - 1 - y;
                offscreen_canvas->SetPixel(x, screen_y, 255, 255, 255);
            }
        }
    }
    
    // Update and draw snowflakes
    float wind_force = wind->get();
    float speed_mult = fall_speed->get();
    
    for (auto& flake : snowflakes) {
        // Update position
        flake.y += flake.vy * dt * speed_mult;
        flake.phase += dt * 2.0f;
        
        // Wind effect (sinusoidal drift)
        flake.vx = wind_force * 10.0f * std::sin(flake.phase);
        flake.x += flake.vx * dt;
        
        // Wrap horizontally
        if (flake.x < 0) flake.x += matrix_width;
        if (flake.x >= matrix_width) flake.x -= matrix_width;
        
        // Check for ground collision or accumulation
        int ix = static_cast<int>(flake.x);
        int iy = static_cast<int>(flake.y);
        
        if (iy >= matrix_height) {
            if (accumulate->get() && ix >= 0 && ix < matrix_width) {
                ground_height[ix] = std::min(ground_height[ix] + 1, static_cast<uint8_t>(matrix_height));
            }
            resetSnowflake(flake);
        } else if (accumulate->get() && iy >= matrix_height - ground_height[ix]) {
            if (ix >= 0 && ix < matrix_width) {
                ground_height[ix] = std::min(ground_height[ix] + 1, static_cast<uint8_t>(matrix_height));
            }
            resetSnowflake(flake);
        }
        
        // Draw snowflake
        uint8_t brightness = flake.brightness;
        
        // Main pixel
        setPixelSafe(ix, iy, brightness, brightness, brightness);
        
        // Larger snowflakes get extra pixels
        if (flake.size > 1.2f) {
            uint8_t dimmed = brightness * 0.6f;
            setPixelSafe(ix - 1, iy, dimmed, dimmed, dimmed);
            setPixelSafe(ix + 1, iy, dimmed, dimmed, dimmed);
            setPixelSafe(ix, iy - 1, dimmed, dimmed, dimmed);
            setPixelSafe(ix, iy + 1, dimmed, dimmed, dimmed);
        }
    }
    
    return true;
}

void SnowfallScene::register_properties() {
    add_property(density);
    add_property(fall_speed);
    add_property(wind);
    add_property(accumulate);
    add_property(bg_color);
}

std::unique_ptr<Scene, void (*)(Scene *)> SnowfallSceneWrapper::create() {
    return {new SnowfallScene(), [](Scene *scene) { delete scene; }};
}

}

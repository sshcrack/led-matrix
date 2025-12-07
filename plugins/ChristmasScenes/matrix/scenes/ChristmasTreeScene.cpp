#include "ChristmasTreeScene.h"
#include <cmath>
#include <algorithm>

namespace Scenes {

ChristmasTreeScene::ChristmasTreeScene() 
    : rng(std::random_device{}()) {
}

void ChristmasTreeScene::initializeLights() {
    lights.clear();
    
    // Tree dimensions (centered)
    int tree_width = 60;
    int tree_height = 90;
    int tree_x = (matrix_width - tree_width) / 2;
    int tree_y = (matrix_height - tree_height) / 2 + 10;
    
    // Place lights on the tree (avoiding the edges)
    std::uniform_real_distribution<float> phase_dist(0.0f, 6.28f);
    
    // Add lights in a triangular pattern
    for (int tier = 0; tier < 5; tier++) {
        int tier_y = tree_y + tier * 16;
        int tier_width = tree_width - tier * 10;
        int tier_x = tree_x + tier * 5;
        
        int lights_in_tier = 3 + tier;
        for (int i = 0; i < lights_in_tier; i++) {
            TreeLight light;
            light.x = tier_x + (tier_width * i) / (lights_in_tier - 1);
            light.y = tier_y + (std::rand() % 10 - 5);
            light.phase = phase_dist(rng);
            
            // Assign colors based on position
            int color_idx = i % 3;
            if (color_idx == 0) light.color = light_color_1->get();
            else if (color_idx == 1) light.color = light_color_2->get();
            else light.color = light_color_3->get();
            
            lights.push_back(light);
        }
    }
}

void ChristmasTreeScene::setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

rgb_matrix::Color ChristmasTreeScene::hsv_to_rgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    return rgb_matrix::Color(
        static_cast<uint8_t>((r + m) * 255),
        static_cast<uint8_t>((g + m) * 255),
        static_cast<uint8_t>((b + m) * 255)
    );
}

void ChristmasTreeScene::drawTree() {
    auto tree_col = tree_color->get();
    auto trunk_col = trunk_color->get();
    
    // Tree dimensions (centered)
    int tree_width = 60;
    int tree_height = 90;
    int tree_x = (matrix_width - tree_width) / 2;
    int tree_y = (matrix_height - tree_height) / 2 + 10;
    
    // Draw triangular tree shape
    for (int y = 0; y < tree_height; y++) {
        float progress = static_cast<float>(y) / tree_height;
        int width_at_y = static_cast<int>(tree_width * (1.0f - progress));
        int x_offset = (tree_width - width_at_y) / 2;
        
        for (int x = 0; x < width_at_y; x++) {
            setPixelSafe(tree_x + x_offset + x, tree_y + y, tree_col.r, tree_col.g, tree_col.b);
        }
    }
    
    // Draw trunk (centered at bottom)
    int trunk_width = 12;
    int trunk_height = 15;
    int trunk_x = (matrix_width - trunk_width) / 2;
    int trunk_y = tree_y + tree_height;
    
    for (int y = 0; y < trunk_height; y++) {
        for (int x = 0; x < trunk_width; x++) {
            setPixelSafe(trunk_x + x, trunk_y + y, trunk_col.r, trunk_col.g, trunk_col.b);
        }
    }
}

void ChristmasTreeScene::drawStar(float t) {
    if (!star_enabled->get()) return;
    
    // Star position (top of tree)
    int star_x = matrix_width / 2;
    int star_y = (matrix_height - 90) / 2 + 5;
    
    // Pulsing effect
    float pulse = 0.7f + 0.3f * std::sin(t * 3.0f);
    uint8_t brightness = static_cast<uint8_t>(255 * pulse);
    
    // Draw star (5-pointed)
    int star_size = 5;
    for (int i = 0; i < 5; i++) {
        float angle = -M_PI / 2 + (2 * M_PI * i) / 5;
        int x1 = star_x + static_cast<int>(star_size * std::cos(angle));
        int y1 = star_y + static_cast<int>(star_size * std::sin(angle));
        
        // Draw line from center to point
        for (float t = 0; t <= 1.0f; t += 0.1f) {
            int x = star_x + static_cast<int>(t * (x1 - star_x));
            int y = star_y + static_cast<int>(t * (y1 - star_y));
            setPixelSafe(x, y, brightness, brightness, 0);
        }
    }
    
    // Center of star
    setPixelSafe(star_x, star_y, brightness, brightness, 100);
}

bool ChristmasTreeScene::render(RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    float dt = frame.dt;
    float t = frame.t;
    
    // Initialize lights on first frame
    if (lights.empty()) {
        initializeLights();
    }
    
    // Clear background (dark blue/black)
    offscreen_canvas->Fill(0, 0, 15);
    
    // Draw tree structure
    drawTree();
    
    // Draw star on top
    drawStar(t);
    
    // Update and draw lights
    float speed = blink_speed->get();
    int pattern = light_pattern->get();
    
    for (size_t i = 0; i < lights.size(); i++) {
        auto& light = lights[i];
        light.phase += dt * speed;
        
        float brightness = 1.0f;
        
        // Different blinking patterns
        if (pattern == 0) {
            // Smooth pulse
            brightness = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(light.phase));
        } else if (pattern == 1) {
            // Alternating blink
            if (static_cast<int>(t * speed * 2) % 2 == i % 2) {
                brightness = 1.0f;
            } else {
                brightness = 0.2f;
            }
        } else if (pattern == 2) {
            // Random twinkle
            brightness = 0.2f + 0.8f * (0.5f + 0.5f * std::sin(light.phase * 2.5f));
        }
        
        rgb_matrix::Color color;
        if (rainbow_mode->get()) {
            // Rainbow mode - cycle through hue
            float hue = std::fmod(t * 60.0f + i * 30.0f, 360.0f);
            color = hsv_to_rgb(hue, 1.0f, brightness);
        } else {
            color = rgb_matrix::Color(
                static_cast<uint8_t>(light.color.r * brightness),
                static_cast<uint8_t>(light.color.g * brightness),
                static_cast<uint8_t>(light.color.b * brightness)
            );
        }
        
        // Draw light with glow effect
        setPixelSafe(light.x, light.y, color.r, color.g, color.b);
        
        // Add glow around bright lights
        if (brightness > 0.7f) {
            uint8_t glow = static_cast<uint8_t>((brightness - 0.7f) * 255 * 0.5f);
            setPixelSafe(light.x - 1, light.y, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f);
            setPixelSafe(light.x + 1, light.y, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f);
            setPixelSafe(light.x, light.y - 1, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f);
            setPixelSafe(light.x, light.y + 1, color.r * 0.5f, color.g * 0.5f, color.b * 0.5f);
        }
    }
    
    return true;
}

void ChristmasTreeScene::register_properties() {
    add_property(blink_speed);
    add_property(rainbow_mode);
    add_property(light_color_1);
    add_property(light_color_2);
    add_property(light_color_3);
    add_property(tree_color);
    add_property(trunk_color);
    add_property(star_enabled);
    add_property(light_pattern);
}

std::unique_ptr<Scene, void (*)(Scene *)> ChristmasTreeSceneWrapper::create() {
    return {new ChristmasTreeScene(), [](Scene *scene) { delete scene; }};
}

}

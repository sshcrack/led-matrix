#include "StarField.h"
#include <cmath>
#include <random>

void StarField::initialize(int width, int height, float speed_factor) {
    // Clear any existing stars
    stars.clear();
    
    matrix_width = width;
    matrix_height = height;
    animation_speed_factor = speed_factor;
    
    // Use fixed seed for consistent star pattern
    std::mt19937 gen(42);
    std::uniform_int_distribution<> x_dist(0, width - 1);
    std::uniform_int_distribution<> y_dist(0, height / 2 + 4); // Stars mainly in upper portion
    std::uniform_real_distribution<float> brightness_dist(0.5f, 1.0f);
    std::uniform_real_distribution<float> twinkle_dist(0.2f, 1.5f); // Twinkle speed in Hz (cycles per second)
    std::uniform_real_distribution<float> phase_dist(0.0f, 2.0f * M_PI);
    std::uniform_int_distribution<> color_dist(0, 19); // For different star colors
    
    // Create star density based on matrix size
    float density = 0.08f; // Approximately 8% of pixels have stars
    int star_count = static_cast<int>(width * height * density);
    
    // Create stars
    for (int i = 0; i < star_count; ++i) {
        Star star;
        star.x = x_dist(gen);
        star.y = y_dist(gen);
        star.max_brightness = brightness_dist(gen);
        star.brightness = star.max_brightness * brightness_dist(gen); // Start at random brightness
        star.twinkle_speed = twinkle_dist(gen); // Hz - cycles per second
        star.phase = phase_dist(gen); // Random starting phase
        
        // Different star colors with realistic distribution
        int color_roll = color_dist(gen);
        if (color_roll < 12) star.color_type = 0;      // White (60%)
        else if (color_roll < 16) star.color_type = 1; // Blue (20%)
        else if (color_roll < 19) star.color_type = 2; // Yellow (15%)
        else star.color_type = 3;                      // Red (5%)
        
        // Some stars are bright enough to have a glow effect (about 10%)
        star.is_bright = (color_roll % 10 == 0);
        
        stars.push_back(star);
    }
    
    initialized = true;
}

void StarField::clear() {
    stars.clear();
    initialized = false;
}

void StarField::update(float delta_time) {
    // Update each star's twinkle animation based on actual elapsed time
    for (auto& star : stars) {
        // Progress the star's phase based on its twinkle speed (in Hz) and time passed
        float phase_increment = star.twinkle_speed * delta_time * 2.0f * M_PI;
        star.phase += phase_increment;
        
        // Keep phase in [0, 2π)
        if (star.phase > 2.0f * M_PI) {
            star.phase -= 2.0f * M_PI;
        }
        
        // Calculate brightness using a smoothed sine wave pattern
        // Map sine wave from [-1, 1] to [0.35, 1] * max_brightness to maintain visibility
        float raw_sine = sin(star.phase);
        
        // Apply smoothstep function to make transitions more natural
        float t = (raw_sine + 1.0f) * 0.5f; // Map from [-1,1] to [0,1]
        float smoothed = t * t * (3.0f - 2.0f * t); // Smoothstep function
        
        // Remap to desired brightness range
        star.brightness = (0.35f + 0.65f * smoothed) * star.max_brightness;
    }
}

void StarField::render(Canvas* canvas) {
    // Draw each star with smooth twinkling effect
    for (const auto& star : stars) {
        // Calculate color based on star type and current brightness
        uint8_t r, g, b;
        uint8_t intensity = static_cast<uint8_t>(star.brightness * 255);
        
        // Apply different colors based on star type
        switch (star.color_type) {
            case 1: // Blue-ish star
                r = intensity - 40;
                g = intensity - 20;
                b = intensity;
                break;
            case 2: // Yellow-ish star
                r = intensity;
                g = intensity - 10;
                b = intensity - 50;
                break;
            case 3: // Red-ish star
                r = intensity;
                g = intensity - 40;
                b = intensity - 40;
                break;
            default: // White star
                r = g = b = intensity;
        }
        
        // Make sure we don't underflow uint8_t values
        r = std::max(r, static_cast<uint8_t>(0));
        g = std::max(g, static_cast<uint8_t>(0));
        b = std::max(b, static_cast<uint8_t>(0));
        
        // Draw the star
        canvas->SetPixel(star.x, star.y, r, g, b);
        
        // Add glow effect for bright stars
        if (star.is_bright && star.brightness > 0.7f) {
            // Calculate glow intensity (dimmer than the star itself)
            uint8_t glow_intensity = static_cast<uint8_t>(star.brightness * 90);
            
            // Add glow in plus shape around star
            for (const auto& [dx, dy] : std::vector<std::pair<int, int>>{
                {0, 1}, {1, 0}, {0, -1}, {-1, 0}
            }) {
                int gx = star.x + dx;
                int gy = star.y + dy;
                
                // Only draw if within bounds
                if (gx >= 0 && gx < matrix_width && gy >= 0 && gy < matrix_height) {
                    uint8_t gr = glow_intensity;
                    uint8_t gg = glow_intensity;
                    uint8_t gb = glow_intensity;
                    
                    // Tint the glow to match the star's color
                    if (star.color_type == 1) gb += 10; // Blue tint
                    else if (star.color_type == 2) gr += 10; // Yellow/orange tint
                    else if (star.color_type == 3) gr += 15; // Red tint
                    
                    canvas->SetPixel(gx, gy, gr, gg, gb);
                }
            }
        }
    }
}

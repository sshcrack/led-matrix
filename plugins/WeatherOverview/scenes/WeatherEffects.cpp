#include "WeatherEffects.h"
#include <cmath>
#include <random>

void WeatherEffects::initialize(int width, int height) {
    matrix_width = width;
    matrix_height = height;
    wind_offset = 0;
    sun_ray_length = 3;
    ray_growing = true;
}

void WeatherEffects::update_wind(int frame_counter, float speed_factor) {
    // Move clouds slowly across the screen
    // Adjust movement speed based on weather conditions
    if (frame_counter % std::max(1, static_cast<int>(2 / speed_factor)) == 0) {
        wind_offset = (wind_offset + 1) % (matrix_width * 2);
    }
}

void WeatherEffects::draw_rain(Canvas* canvas, const std::vector<WeatherParticle>& particles) {
    // Draw raindrops
    for (const auto& p : particles) {
        // Only draw particles that are on screen
        if (p.y >= 0 && p.y < matrix_height) {
            // Draw vertical rain streak
            canvas->SetPixel(p.x, p.y, 120, 180, 255); // Light blue rain
            if (p.y > 0) {
                canvas->SetPixel(p.x, p.y - 1, 100, 150, 220); // Fainter streak above
            }
        }
    }
}

void WeatherEffects::draw_snow(Canvas* canvas, const std::vector<WeatherParticle>& particles) {
    // Draw snowflakes
    for (const auto& p : particles) {
        if (p.y >= 0 && p.y < matrix_height) {
            // Draw a small snowflake (single white pixel)
            canvas->SetPixel(p.x, p.y, 255, 255, 255);
            
            // For some particles, draw a larger flake pattern
            if (p.x % 3 == 0) {
                if (p.x > 0) canvas->SetPixel(p.x - 1, p.y, 220, 220, 220);
                if (p.x < matrix_width-1) canvas->SetPixel(p.x + 1, p.y, 220, 220, 220);
                if (p.y > 0) canvas->SetPixel(p.x, p.y - 1, 220, 220, 220);
                if (p.y < matrix_height-1) canvas->SetPixel(p.x, p.y + 1, 220, 220, 220);
            }
        }
    }
}

void WeatherEffects::draw_clouds(Canvas* canvas, int offset, bool dark, bool is_night) {
    // Define cloud shapes - we'll use multiple cloud blocks
    std::vector<std::vector<int>> cloud_shapes = {
        // First cloud pattern
        {0, 0, 1, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 2, 2, 1, 1, 1, 0, 0},
        {1, 1, 2, 2, 2, 2, 2, 1, 1, 0},
        {1, 2, 2, 2, 2, 2, 2, 2, 1, 0},
        {0, 1, 1, 2, 2, 2, 1, 1, 0, 0},
        
        // Second cloud pattern (smaller)
        {0, 0, 1, 1, 0, 0},
        {0, 1, 2, 2, 1, 0},
        {1, 2, 2, 2, 2, 1},
        {0, 1, 1, 1, 0, 0}
    };

    // Cloud positions - x offset, y position, pattern number (0 or 1)
    std::vector<std::tuple<int, int, int>> cloud_positions = {
        {5, 5, 0},  // First cloud pattern at position (5, 5)
        {25, 8, 1}, // Second cloud pattern at position (25, 8)
        {40, 3, 0}  // First cloud pattern again at position (40, 3)
    };
    
    // Cloud colors - adjust for night time if needed
    uint8_t r1, g1, b1, r2, g2, b2;
    if (dark) {
        r1 = 70; g1 = 70; b1 = 90;  // Dark clouds outer
        r2 = 50; g2 = 50; b2 = 70;  // Dark clouds inner
    } else {
        r1 = 160; g1 = 160; b1 = 180; // Light clouds outer
        r2 = 200; g2 = 200; b2 = 220; // Light clouds inner
    }
    
    // Make clouds darker at night
    if (is_night) {
        r1 = static_cast<uint8_t>(r1 * 0.7f);
        g1 = static_cast<uint8_t>(g1 * 0.7f);
        b1 = static_cast<uint8_t>(b1 * 0.8f); // Preserve some blue for moonlit clouds
        
        r2 = static_cast<uint8_t>(r2 * 0.7f);
        g2 = static_cast<uint8_t>(g2 * 0.7f);
        b2 = static_cast<uint8_t>(b2 * 0.8f);
    }
    
    // Draw each cloud
    for (const auto& [base_x, y, pattern_idx] : cloud_positions) {
        int x_offset = (base_x + offset / 2) % (matrix_width + 20) - 10;
        int pattern_offset = pattern_idx == 0 ? 0 : 5;
        int pattern_height = pattern_idx == 0 ? 5 : 4;
        int pattern_width = pattern_idx == 0 ? 10 : 6;
        
        for (int row = 0; row < pattern_height; ++row) {
            for (int col = 0; col < pattern_width; ++col) {
                int cloud_x = x_offset + col;
                int cloud_y = y + row;
                int pixel_type = cloud_shapes[pattern_offset + row][col];
                
                if (cloud_x >= 0 && cloud_x < matrix_width && cloud_y >= 0 && cloud_y < matrix_height && pixel_type > 0) {
                    // Draw cloud pixel - type 1 is edge, type 2 is inner part
                    if (pixel_type == 1) {
                        canvas->SetPixel(cloud_x, cloud_y, r1, g1, b1);
                    } else {
                        canvas->SetPixel(cloud_x, cloud_y, r2, g2, b2);
                    }
                }
            }
        }
    }
}

void WeatherEffects::draw_sun(Canvas* canvas, int frame_counter, float speed_factor) {
    // Sun position
    int sun_x = matrix_width / 4;
    int sun_y = matrix_height / 3;
    int sun_radius = 4;
    
    // Animate sun rays with weather-adjusted timing
    if (frame_counter % std::max(1, static_cast<int>(5 / speed_factor)) == 0) {
        if (ray_growing) {
            sun_ray_length++;
            if (sun_ray_length >= 6) ray_growing = false;
        } else {
            sun_ray_length--;
            if (sun_ray_length <= 3) ray_growing = true;
        }
    }
    
    // Draw sun circle
    for (int y = -sun_radius; y <= sun_radius; y++) {
        for (int x = -sun_radius; x <= sun_radius; x++) {
            if (x*x + y*y <= sun_radius*sun_radius) {
                int pixel_x = sun_x + x;
                int pixel_y = sun_y + y;
                if (pixel_x >= 0 && pixel_x < matrix_width && pixel_y >= 0 && pixel_y < matrix_height) {
                    canvas->SetPixel(pixel_x, pixel_y, 255, 220, 0);
                }
            }
        }
    }
    
    // Draw rays
    for (int angle = 0; angle < 360; angle += 45) {
        float rad = angle * M_PI / 180.0;
        int ray_x = sun_x + (sun_radius + 2) * cos(rad);
        int ray_y = sun_y + (sun_radius + 2) * sin(rad);
        int ray_end_x = sun_x + (sun_radius + sun_ray_length) * cos(rad);
        int ray_end_y = sun_y + (sun_radius + sun_ray_length) * sin(rad);
        
        // Draw a line for the ray
        for (float t = 0; t <= 1.0; t += 0.25) {
            int x = ray_x + t * (ray_end_x - ray_x);
            int y = ray_y + t * (ray_end_y - ray_y);
            if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                canvas->SetPixel(x, y, 255, 220, 0);
            }
        }
    }
}

void WeatherEffects::draw_moon(Canvas* canvas, bool with_glow) {
    // Moon position
    int moon_x = matrix_width / 4;
    int moon_y = matrix_height / 3;
    int moon_radius = 4;
    int highlight_offset = 1; // Creates a crescent effect
    
    // Draw moon circle
    for (int y = -moon_radius; y <= moon_radius; y++) {
        for (int x = -moon_radius; x <= moon_radius; x++) {
            if (x*x + y*y <= moon_radius*moon_radius) {
                int pixel_x = moon_x + x;
                int pixel_y = moon_y + y;
                
                // Create crescent effect by making part of the moon darker
                bool is_highlight = (x + highlight_offset)*(x + highlight_offset) + y*y <= (moon_radius - 1)*(moon_radius - 1);
                
                if (pixel_x >= 0 && pixel_x < matrix_width && pixel_y >= 0 && pixel_y < matrix_height) {
                    if (is_highlight) {
                        // Darker part of the moon
                        canvas->SetPixel(pixel_x, pixel_y, 120, 120, 150);
                    } else {
                        // Brighter part of the moon
                        canvas->SetPixel(pixel_x, pixel_y, 220, 220, 230);
                    }
                }
            }
        }
    }
    
    // Add subtle glow around the moon if requested
    if (with_glow) {
        for (int y = -moon_radius-2; y <= moon_radius+2; y++) {
            for (int x = -moon_radius-2; x <= moon_radius+2; x++) {
                // Only draw glow for pixels outside the moon but within glow radius
                int dist_squared = x*x + y*y;
                if (dist_squared > moon_radius*moon_radius && dist_squared <= (moon_radius+2)*(moon_radius+2)) {
                    int pixel_x = moon_x + x;
                    int pixel_y = moon_y + y;
                    
                    if (pixel_x >= 0 && pixel_x < matrix_width && pixel_y >= 0 && pixel_y < matrix_height) {
                        // Calculate glow intensity - stronger closer to the moon
                        float intensity = 1.0f - (sqrt(dist_squared) - moon_radius) / 2.0f;
                        uint8_t glow = static_cast<uint8_t>(intensity * 40); // Subtle glow
                        
                        // Add glow to existing pixel color
                        canvas->SetPixel(pixel_x, pixel_y, glow, glow, glow + 10);
                    }
                }
            }
        }
    }
}

void WeatherEffects::draw_lightning(Canvas* canvas, bool active, int wind_offset, bool is_night, 
                                    const std::vector<WeatherParticle>& particles) {
    // Draw dark clouds
    draw_clouds(canvas, wind_offset, true, is_night);
    
    // Draw rain
    draw_rain(canvas, particles);
    
    // Process active lightning
    if (active) {
        // Flash the entire screen
        for (int y = 0; y < matrix_height; y++) {
            for (int x = 0; x < matrix_width; x++) {
                canvas->SetPixel(x, y, 200, 200, 150); // Flash color
            }
        }
    }
}

void WeatherEffects::draw_fog(Canvas* canvas, int frame_counter, bool is_night) {
    // Draw multiple horizontal fog layers
    for (int layer = 0; layer < 5; layer++) {
        int y = 5 + layer * (matrix_height / 6);
        int intensity = 120 + ((frame_counter + layer * 10) % 40) * 2;
        if (intensity > 220) intensity = 440 - intensity; // Oscillate between 120-220
        
        // Adjust fog intensity for night time
        if (is_night) {
            intensity = static_cast<int>(intensity * 0.7f);
        }
        
        for (int x = 0; x < matrix_width; x++) {
            // Create a wavy fog pattern
            int wave = 2 * sin(0.1 * x + 0.05 * frame_counter + layer);
            int fog_y = y + wave;
            if (fog_y >= 0 && fog_y < matrix_height) {
                canvas->SetPixel(x, fog_y, intensity, intensity, intensity);
            }
        }
    }
}

void WeatherEffects::draw_clear_sky(Canvas* canvas, bool night) {
    // Draw sky background
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            if (night) {
                // Night sky gradient from dark blue to black
                int blue = 50 - (y * 50 / matrix_height);
                if (blue < 0) blue = 0;
                canvas->SetPixel(x, y, 0, 0, blue);
            } else {
                // Day sky gradient from light blue to darker blue
                int blue = 180 - (y * 80 / matrix_height);
                if (blue < 100) blue = 100;
                canvas->SetPixel(x, y, 100, 150, blue);
            }
        }
    }
}
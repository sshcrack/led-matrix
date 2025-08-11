#include "ClockScene.h"
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace AmbientScenes {

ClockScene::ClockScene()
    : Scene(), current_hour(0.0f), current_minute(0.0f), current_second(0.0f), last_digit_update(0) {
}

void ClockScene::initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
    Scene::initialize(matrix, l_offscreen_canvas);
    
    // Initialize clock hands to current time
    auto now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    current_hour = local_time->tm_hour % 12;
    current_minute = local_time->tm_min;
    current_second = local_time->tm_sec;
}

bool ClockScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    // Get current time
    auto now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    int hour = local_time->tm_hour % 12;
    int minute = local_time->tm_min;
    int second = local_time->tm_sec;
    
    // Update smooth transitions if enabled
    if (smooth_motion->get()) {
        // Smooth transition to new positions
        const float transition_speed = 0.15f;
        current_hour = current_hour + (hour + minute/60.0f - current_hour) * transition_speed;
        current_minute = current_minute + (minute + second/60.0f - current_minute) * transition_speed;
        current_second = current_second + (second - current_second) * transition_speed;
    } else {
        current_hour = hour + minute/60.0f;
        current_minute = minute;
        current_second = second;
    }

    // Clear the canvas
    auto bg = bg_color->get();
    offscreen_canvas->Fill(bg.r, bg.g, bg.b);
    
    // Calculate center and radius for analog clock
    int center_x = matrix->width() / 2;
    int center_y = matrix->height() / 2;
    int radius = std::min(center_x, center_y) - 1;
    
    // Adjust layout based on what's being shown
    bool showing_analog = show_analog->get();
    bool showing_digital = show_digital->get();
    bool showing_date = show_date->get();
    
    // Draw analog clock if enabled
    if (showing_analog) {
        // If we're showing both analog and digital, make the analog clock smaller
        if (showing_digital) {
            center_y = matrix->height() / 3;
            radius = std::min(center_x, center_y) - 1;
        }
        draw_analog_clock(matrix, center_x, center_y, radius);
    }
    
    // Draw digital clock if enabled
    if (showing_digital) {
        int y_position;
        if (showing_analog) {
            y_position = matrix->height() - 10; // Lower on the display
        } else {
            y_position = (matrix->height() - 5) / 2; // Center vertically
        }
        draw_digital_clock(matrix, y_position);
    }
    
    // Draw date if enabled
    if (showing_date) {
        int y_position;
        if (showing_analog && showing_digital) {
            y_position = matrix->height() - 5; // At the very bottom
        } else if (showing_digital) {
            y_position = matrix->height() - 10; // Below digital clock
        } else if (showing_analog) {
            y_position = matrix->height() - 5; // Below analog clock
        } else {
            y_position = (matrix->height() - 5) / 2; // Center vertically
        }
        draw_date(matrix, y_position);
    }

    return true;
}

void ClockScene::draw_analog_clock(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, int radius) {
    // Draw the clock face based on selected style
    draw_clock_face(matrix, center_x, center_y, radius);
    
    // Calculate hand angles
    float hour_angle = (current_hour / (12.0f)) * 2.0f * M_PI - M_PI_2;
    float minute_angle = (current_minute / 60.0f) * 2.0f * M_PI - M_PI_2;
    float second_angle = (current_second / 60.0f) * 2.0f * M_PI - M_PI_2;
    
    // Calculate hand endpoints
    int hour_x = center_x + static_cast<int>(sin(hour_angle) * radius * 0.5);
    int hour_y = center_y - static_cast<int>(cos(hour_angle) * radius * 0.5);
    
    int minute_x = center_x + static_cast<int>(sin(minute_angle) * radius * 0.7);
    int minute_y = center_y - static_cast<int>(cos(minute_angle) * radius * 0.7);
    
    int second_x = center_x + static_cast<int>(sin(second_angle) * radius * 0.8);
    int second_y = center_y - static_cast<int>(cos(second_angle) * radius * 0.8);
    
    auto h_color = hour_color->get();
    auto m_color = minute_color->get();
    auto s_color = second_color->get();

    // Draw hour hand (thicker)
    if (use_antialiasing->get()) {
        draw_antialiased_line(matrix, center_x, center_y, hour_x, hour_y,
                             h_color.r, h_color.g, h_color.b, 
                             use_glow_effect->get());
        
        // Make hour hand thicker by drawing additional pixels
        int perpendicular_dx = static_cast<int>(-cos(hour_angle));
        int perpendicular_dy = static_cast<int>(-sin(hour_angle));
        
        draw_antialiased_line(matrix, center_x + perpendicular_dx, center_y + perpendicular_dy, 
                             hour_x + perpendicular_dx, hour_y + perpendicular_dy,
                             h_color.r, h_color.g, h_color.b, 
                             false);
    } else {
        draw_clock_hand(matrix, center_x, center_y, hour_angle, radius * 0.5, 
                       h_color.r, h_color.g, h_color.b, 2);
    }
    
    // Draw minute hand
    if (use_antialiasing->get()) {
        draw_antialiased_line(matrix, center_x, center_y, minute_x, minute_y,
                             m_color.r, m_color.g, m_color.b, 
                             use_glow_effect->get());
    } else {
        draw_clock_hand(matrix, center_x, center_y, minute_angle, radius * 0.7, 
                        m_color.r, m_color.g, m_color.b, 1);
    }
    
    // Draw second hand (if enabled)
    if (show_seconds->get()) {
        if (use_antialiasing->get()) {
            draw_antialiased_line(matrix, center_x, center_y, second_x, second_y,
                                 s_color.r, s_color.g, s_color.b, 
                                 use_glow_effect->get());
        } else {
            draw_clock_hand(matrix, center_x, center_y, second_angle, radius * 0.8, 
                           s_color.r, s_color.g, s_color.b, 1);
        }
    }
    
    // Center dot with controlled glow
    offscreen_canvas->SetPixel(center_x, center_y, 255, 255, 255);
    if (use_glow_effect->get()) {
        apply_glow(center_x, center_y, 255, 255, 255, 0.8f);
    }
}

void ClockScene::draw_clock_hand(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, 
                                float angle, int length, uint8_t r, uint8_t g, uint8_t b, int thickness) {
    float cos_angle = cos(angle);
    float sin_angle = sin(angle);
    
    // Draw line using Bresenham's algorithm
    int x1 = center_x;
    int y1 = center_y;
    int x2 = center_x + static_cast<int>(sin_angle * length);
    int y2 = center_y - static_cast<int>(cos_angle * length);
    
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        // Draw the main pixel
        offscreen_canvas->SetPixel(x1, y1, r, g, b);
        
        // For thicker lines (thickness > 1)
        if (thickness > 1) {
            offscreen_canvas->SetPixel(x1 + 1, y1, r, g, b);
            offscreen_canvas->SetPixel(x1, y1 + 1, r, g, b);
            offscreen_canvas->SetPixel(x1 + 1, y1 + 1, r, g, b);
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void ClockScene::draw_digital_clock(rgb_matrix::RGBMatrixBase *matrix, int y_position) {
    auto now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    int hour = local_time->tm_hour;
    int minute = local_time->tm_min;
    int second = local_time->tm_sec;
    
    bool is_pm = hour >= 12;
    hour = hour % 12;
    if (hour == 0) hour = 12;  // 12-hour format
    
    // Calculate horizontal centering based on matrix width
    int x_start = (matrix->width() - 24) / 2;  // Adjust as needed for digit spacing
    
    // Draw hour digits
    auto h_color = hour_color->get();
    draw_small_digit(matrix, hour / 10, x_start, y_position, 
                    h_color.r, h_color.g, h_color.b);
    draw_small_digit(matrix, hour % 10, x_start + 5, y_position, 
                    h_color.r, h_color.g, h_color.b);
    
    // Draw colon (blinking)
    if (second % 2 == 0 || !show_seconds->get()) {
        offscreen_canvas->SetPixel(x_start + 9, y_position + 1, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 9, y_position + 3, 255, 255, 255);
    }
    
    auto m_color = minute_color->get();
    // Draw minute digits
    draw_small_digit(matrix, minute / 10, x_start + 11, y_position, 
                    m_color.r, m_color.g, m_color.b);
    draw_small_digit(matrix, minute % 10, x_start + 16, y_position, 
                    m_color.r, m_color.g, m_color.b);
    
    // Draw AM/PM indicator
    if (is_pm) {
        offscreen_canvas->SetPixel(x_start + 22, y_position, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 22, y_position + 1, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 23, y_position, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 23, y_position + 1, 255, 255, 255);
    } else {
        offscreen_canvas->SetPixel(x_start + 22, y_position + 3, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 22, y_position + 4, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 23, y_position + 3, 255, 255, 255);
        offscreen_canvas->SetPixel(x_start + 23, y_position + 4, 255, 255, 255);
    }
}

void ClockScene::draw_small_digit(rgb_matrix::RGBMatrixBase *matrix, int digit, int x, int y, 
                                 uint8_t r, uint8_t g, uint8_t b) {
    // Define small 3x5 digit patterns
    static const bool digit_patterns[10][5][3] = {
        // 0
        {
            {1, 1, 1},
            {1, 0, 1},
            {1, 0, 1},
            {1, 0, 1},
            {1, 1, 1}
        },
        // 1
        {
            {0, 1, 0},
            {1, 1, 0},
            {0, 1, 0},
            {0, 1, 0},
            {1, 1, 1}
        },
        // 2
        {
            {1, 1, 1},
            {0, 0, 1},
            {1, 1, 1},
            {1, 0, 0},
            {1, 1, 1}
        },
        // 3
        {
            {1, 1, 1},
            {0, 0, 1},
            {1, 1, 1},
            {0, 0, 1},
            {1, 1, 1}
        },
        // 4
        {
            {1, 0, 1},
            {1, 0, 1},
            {1, 1, 1},
            {0, 0, 1},
            {0, 0, 1}
        },
        // 5
        {
            {1, 1, 1},
            {1, 0, 0},
            {1, 1, 1},
            {0, 0, 1},
            {1, 1, 1}
        },
        // 6
        {
            {1, 1, 1},
            {1, 0, 0},
            {1, 1, 1},
            {1, 0, 1},
            {1, 1, 1}
        },
        // 7
        {
            {1, 1, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1},
            {0, 0, 1}
        },
        // 8
        {
            {1, 1, 1},
            {1, 0, 1},
            {1, 1, 1},
            {1, 0, 1},
            {1, 1, 1}
        },
        // 9
        {
            {1, 1, 1},
            {1, 0, 1},
            {1, 1, 1},
            {0, 0, 1},
            {1, 1, 1}
        }
    };
    
    // Ensure digit is in range
    if (digit < 0 || digit > 9) digit = 0;
    
    // Draw the digit
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (digit_patterns[digit][row][col]) {
                offscreen_canvas->SetPixel(x + col, y + row, r, g, b);
            }
        }
    }
}

void ClockScene::draw_clock_face(rgb_matrix::RGBMatrixBase *matrix, int center_x, int center_y, int radius) {
    auto style = clock_style->get().get();
    
    // Draw based on selected style
    if (style == ClockStyle::CLASSIC) { // Classic style
        // Draw hour markers
        for (int i = 0; i < 12; i++) {
            float angle = i * M_PI / 6.0f;
            int marker_length = (i % 3 == 0) ? 3 : 2; // Longer markers at 12, 3, 6, 9
            
            int outer_x = center_x + static_cast<int>(sin(angle) * radius);
            int outer_y = center_y - static_cast<int>(cos(angle) * radius);
            int inner_x = center_x + static_cast<int>(sin(angle) * (radius - marker_length));
            int inner_y = center_y - static_cast<int>(cos(angle) * (radius - marker_length));
            
            // Draw marker line
            draw_antialiased_line(matrix, outer_x, outer_y, inner_x, inner_y, 200, 200, 200, false);
        }
    } 
    else if (style == ClockStyle::MINIMAL) { // Minimal style
        // Just draw dots at hour positions
        for (int i = 0; i < 12; i++) {
            float angle = i * M_PI / 6.0f;
            int x = center_x + static_cast<int>(sin(angle) * radius);
            int y = center_y - static_cast<int>(cos(angle) * radius);
            
            // Larger dots at 3, 6, 9, 12
            if (i % 3 == 0) {
                offscreen_canvas->SetPixel(x, y, 200, 200, 200);
                // Add adjacent pixels for larger markers
                offscreen_canvas->SetPixel(x-1, y, 150, 150, 150);
                offscreen_canvas->SetPixel(x+1, y, 150, 150, 150);
                offscreen_canvas->SetPixel(x, y-1, 150, 150, 150);
                offscreen_canvas->SetPixel(x, y+1, 150, 150, 150);
            } else {
                offscreen_canvas->SetPixel(x, y, 100, 100, 100);
            }
        }
    }
    else if (style == ClockStyle::ELEGANT) { // Elegant style
        // Draw a circle outline
        int segments = 60;
        for (int i = 0; i < segments; i++) {
            float angle = i * 2.0f * M_PI / segments;
            int x = center_x + static_cast<int>(sin(angle) * radius);
            int y = center_y - static_cast<int>(cos(angle) * radius);
            
            // Vary brightness to create a subtle gradient effect
            int brightness = 100 + 100 * (1 + sin(angle * 2)) / 2;
            offscreen_canvas->SetPixel(x, y, brightness, brightness, brightness);
        }
        
        // Draw hour markers
        for (int i = 0; i < 12; i++) {
            float angle = i * M_PI / 6.0f;
            int x = center_x + static_cast<int>(sin(angle) * (radius - 1));
            int y = center_y - static_cast<int>(cos(angle) * (radius - 1));
            
            if (i % 3 == 0) { // 12, 3, 6, 9 positions
                offscreen_canvas->SetPixel(x, y, 255, 255, 255);
                
                // Add adjacent pixels for larger markers
                int dx = static_cast<int>(sin(angle));
                int dy = static_cast<int>(-cos(angle));
                offscreen_canvas->SetPixel(x + dx, y + dy, 200, 200, 200);
                offscreen_canvas->SetPixel(x - dx, y - dy, 200, 200, 200);
            } else {
                offscreen_canvas->SetPixel(x, y, 180, 180, 180);
            }
        }
    }
}

std::string ClockScene::get_name() const {
    return "clock";
}

void ClockScene::register_properties() {
    add_property(show_digital);
    add_property(show_analog);
    add_property(show_seconds);
    add_property(show_date);
    add_property(hour_color);
    add_property(minute_color);
    add_property(second_color);
    add_property(bg_color);
    add_property(smooth_motion);
    add_property(clock_style);
    add_property(use_antialiasing);
    add_property(use_glow_effect);
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> ClockSceneWrapper::create() {
    return {new ClockScene(), [](Scenes::Scene *scene) {
        delete (ClockScene *) scene;
    }};
}

void ClockScene::set_pixel_with_brightness(int x, int y, uint8_t r, uint8_t g, uint8_t b, float brightness) {
    if (x < 0 || y < 0 || x >= offscreen_canvas->width() || y >= offscreen_canvas->height()) {
        return;
    }
    
    // Apply brightness factor (0.0 to 1.0)
    uint8_t br = static_cast<uint8_t>(r * brightness);
    uint8_t bg = static_cast<uint8_t>(g * brightness);
    uint8_t bb = static_cast<uint8_t>(b * brightness);
    
    offscreen_canvas->SetPixel(x, y, br, bg, bb);
}

void ClockScene::apply_glow(int x, int y, uint8_t r, uint8_t g, uint8_t b, float intensity) {
    if (!use_glow_effect->get() || intensity <= 0.1f) {
        return;
    }
    
    // Apply a more controlled glow around the pixel
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue; // Skip the center pixel
            
            int gx = x + dx;
            int gy = y + dy;
            
            // Check bounds
            if (gx < 0 || gx >= offscreen_canvas->width() || 
                gy < 0 || gy >= offscreen_canvas->height()) {
                continue;
            }
            
            // Calculate distance-based intensity
            float dist = sqrt(dx*dx + dy*dy);
            float glow_factor = intensity * (1.0f - dist/1.5f);
            
            if (glow_factor > 0.1f) {
                // Apply dimmer color for glow
                uint8_t gr = static_cast<uint8_t>(r * glow_factor * 0.4f);
                uint8_t gg = static_cast<uint8_t>(g * glow_factor * 0.4f);
                uint8_t gb = static_cast<uint8_t>(b * glow_factor * 0.4f);
                
                offscreen_canvas->SetPixel(gx, gy, gr, gg, gb);
            }
        }
    }
}

void ClockScene::draw_antialiased_line(rgb_matrix::RGBMatrixBase *matrix, int x0, int y0, int x1, int y1, 
                                     uint8_t r, uint8_t g, uint8_t b, bool should_apply_glow) {
    // If antialiasing is disabled, use a simple line algorithm
    if (!use_antialiasing->get()) {
        // Draw line using Bresenham's algorithm
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            offscreen_canvas->SetPixel(x0, y0, r, g, b);
            if (should_apply_glow && use_glow_effect->get()) {
                apply_glow(x0, y0, r, g, b, 0.7f);
            }
            
            if (x0 == x1 && y0 == y1) break;
            
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
        return;
    }
    
    // Simplified anti-aliased line drawing for LED matrix
    // Using a modified Bresenham's algorithm with brightness control
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        // Draw the main pixel at full brightness
        offscreen_canvas->SetPixel(x0, y0, r, g, b);
        
        // Apply glow if requested
        if (should_apply_glow && use_glow_effect->get()) {
            // Use a smaller glow radius for better appearance on LED matrix
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // Skip center pixel
                    
                    int gx = x0 + dx;
                    int gy = y0 + dy;
                    
                    // Check bounds
                    if (gx >= 0 && gx < offscreen_canvas->width() && 
                        gy >= 0 && gy < offscreen_canvas->height()) {
                        
                        // Calculate distance-based intensity
                        float dist = sqrt(dx*dx + dy*dy);
                        float intensity = 0.5f * (1.0f - dist/2.0f);
                        
                        // Apply dimmer color for glow
                        uint8_t gr = static_cast<uint8_t>(r * intensity);
                        uint8_t gg = static_cast<uint8_t>(g * intensity);
                        uint8_t gb = static_cast<uint8_t>(b * intensity);
                        
                        offscreen_canvas->SetPixel(gx, gy, gr, gg, gb);
                    }
                }
            }
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ClockScene::draw_date(rgb_matrix::RGBMatrixBase *matrix, int y_position) {
    auto now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    
    int day = local_time->tm_mday;
    int month = local_time->tm_mon + 1; // tm_mon is 0-11
    
    // Calculate horizontal centering based on matrix width
    int x_start = (matrix->width() - 11) / 2; // Adjust for MM/DD format
    
    // Draw month digits
    draw_small_digit(matrix, month / 10, x_start, y_position, 200, 200, 200);
    draw_small_digit(matrix, month % 10, x_start + 4, y_position, 200, 200, 200);
    
    // Draw separator
    offscreen_canvas->SetPixel(x_start + 7, y_position + 2, 150, 150, 150);
    
    // Draw day digits
    draw_small_digit(matrix, day / 10, x_start + 9, y_position, 200, 200, 200);
    draw_small_digit(matrix, day % 10, x_start + 13, y_position, 200, 200, 200);
}

}

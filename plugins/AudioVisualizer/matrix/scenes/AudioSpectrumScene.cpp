#include "AudioSpectrumScene.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <algorithm>
#include "shared/matrix/plugin_loader/loader.h"

using namespace Scenes;

// Helper function to convert HSV to RGB
void hsv_to_rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (s <= 0.0f) {
        r = g = b = static_cast<uint8_t>(v * 255);
        return;
    }

    h = fmod(h, 360.0f) / 60.0f;
    int hi = static_cast<int>(h);
    float f = h - hi;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (hi) {
        case 0: r = v * 255;
            g = t * 255;
            b = p * 255;
            break;
        case 1: r = q * 255;
            g = v * 255;
            b = p * 255;
            break;
        case 2: r = p * 255;
            g = v * 255;
            b = t * 255;
            break;
        case 3: r = p * 255;
            g = q * 255;
            b = v * 255;
            break;
        case 4: r = t * 255;
            g = p * 255;
            b = v * 255;
            break;
        default: r = v * 255;
            g = p * 255;
            b = q * 255;
            break;
    }
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> AudioSpectrumSceneWrapper::create() {
    return {
        new AudioSpectrumScene(), [](Scenes::Scene *scene) {
            delete scene;
        }
    };
}

AudioSpectrumScene::AudioSpectrumScene() : plugin(nullptr), last_timestamp(0) {
    // Find the AudioVisualizer plugin
    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    for (auto &p: plugins) {
        if (auto av = dynamic_cast<AudioVisualizer *>(p)) {
            plugin = av;
            break;
        }
    }

    if (!plugin) {
        spdlog::error("AudioSpectrumScene: Failed to find AudioVisualizer plugin");
    }
}

string AudioSpectrumScene::get_name() const {
    return "audio_spectrum";
}

void AudioSpectrumScene::register_properties() {
    add_property(bar_width);
    add_property(gap_width);
    add_property(mirror_display);
    add_property(rainbow_colors);
    add_property(base_color);
    add_property(falling_dots);
    add_property(dot_fall_speed);
    add_property(display_mode);
    add_property(gradient_mode);
    add_property(gradient_color1);
    add_property(gradient_color2);
    add_property(smooth_gradient);
    add_property(circle_radius);
    add_property(rotate_visualization);
    add_property(rotation_speed);
}

void AudioSpectrumScene::initialize_if_needed(int num_bands) {
    if (peak_positions.size() != num_bands) {
        peak_positions.resize(num_bands, 0.0f);
    }
}

uint32_t AudioSpectrumScene::get_bar_color(const int band_index, const float intensity, const int num_bands) const {
    if (gradient_mode->get()) {
        // Use gradient coloring
        float position = static_cast<float>(band_index) / num_bands;
        return get_gradient_color(position, intensity);
    } else if (rainbow_colors->get()) {
        // Generate rainbow color based on band index
        const float hue = static_cast<float>(band_index) / num_bands * 360.0f;
        uint8_t r, g, b;
        hsv_to_rgb(hue, 1.0f, intensity, r, g, b);
        return (r << 16) | (g << 8) | b;
    } else {
        // Use base color with intensity
        const rgb_matrix::Color color = base_color->get();
        const uint8_t r = color.r * intensity;
        const uint8_t g = color.g * intensity;
        const uint8_t b = color.b * intensity;
        return (r << 16) | (g << 8) | b;
    }
}

uint32_t AudioSpectrumScene::get_gradient_color(float position, float intensity) const {
    const auto color1 = gradient_color1->get();
    const auto color2 = gradient_color2->get();
    
    uint8_t r, g, b;
    
    if (smooth_gradient->get()) {
        // Smooth interpolation between colors
        r = static_cast<uint8_t>((color1.r * (1.0f - position) + color2.r * position) * intensity);
        g = static_cast<uint8_t>((color1.g * (1.0f - position) + color2.g * position) * intensity);
        b = static_cast<uint8_t>((color1.b * (1.0f - position) + color2.b * position) * intensity);
    } else {
        // Hard transition at midpoint
        if (position < 0.5f) {
            r = color1.r * intensity;
            g = color1.g * intensity;
            b = color1.b * intensity;
        } else {
            r = color2.r * intensity;
            g = color2.g * intensity;
            b = color2.b * intensity;
        }
    }
    
    return (r << 16) | (g << 8) | b;
}

bool AudioSpectrumScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    if (!plugin) {
        spdlog::info("AudioSpectrumScene: Plugin not found, cannot render");
        return false;
    }

    auto frame_time = frameTimer.tick();
    auto audio_data = plugin->get_audio_data();

    if (audio_data.empty()) {
        // No audio data yet, clear the display
        spdlog::error("AudioSpectrumScene: Audio Data is empty, cannot render");
        offscreen_canvas->Clear();
        return false;
    }

    initialize_if_needed(audio_data.size());

    // Get new timestamp to check if the data has been updated
    uint32_t current_timestamp = plugin->get_last_timestamp();
    bool new_data = (current_timestamp != last_timestamp);
    last_timestamp = current_timestamp;

    // Clear the display
    offscreen_canvas->Clear();

    const int width = matrix->width();
    const int height = matrix->height();
    
    // Update rotation angle if rotation is enabled
    if (rotate_visualization->get()) {
        rotation_angle += 0.01f * rotation_speed->get();
        if (rotation_angle > 2 * M_PI) rotation_angle -= 2 * M_PI;
    }

    // Handle special visualization modes
    int mode = display_mode->get();
    if (mode == 3) { // Circle mode
        render_circle_visualization(matrix, audio_data);
        return true;
    } else if (mode == 4) { // Spiral mode
        render_spiral_visualization(matrix, audio_data);
        return true;
    }

    // Continue with standard bar visualizations for modes 0, 1, 2
    // Determine how many bands we can fit on the display
    const int total_width_per_band = bar_width->get() + gap_width->get();
    const int max_bands = width / total_width_per_band;
    const int num_bands = std::min(static_cast<int>(audio_data.size()), max_bands);

    // Update peak positions (falling dots)
    for (int i = 0; i < num_bands; i++) {
        float band_value = audio_data[i] / 255.0f;
        
        // Update peak positions (falling dots)
        if (band_value > peak_positions[i]) {
            peak_positions[i] = band_value;
        } else if (falling_dots->get()) {
            peak_positions[i] -= dot_fall_speed->get() * frame_time.deltaFrame.count();
            peak_positions[i] = std::max(0.0f, peak_positions[i]);
        }
    }

    // Draw the spectrum
    for (int i = 0; i < num_bands; i++) {
        int x;
        
        // Calculate x position based on display mode
        switch(display_mode->get()) {
            case 1: // Center out
                {
                    int half_width = width / 2;
                    int half_bands = num_bands / 2;
                    if (i < half_bands) {
                        // Left side
                        x = half_width - (i + 1) * total_width_per_band;
                    } else {
                        // Right side
                        x = half_width + (i - half_bands) * total_width_per_band;
                    }
                }
                break;
                
            case 2: // Edges to center
                {
                    int half_bands = num_bands / 2;
                    if (i < half_bands) {
                        // Left side (starting from edge)
                        x = i * total_width_per_band;
                    } else {
                        // Right side (starting from edge)
                        x = width - 1 - ((i - half_bands) + 1) * total_width_per_band;
                    }
                }
                break;
                
            case 0: // Normal (left to right)
            default:
                x = i * total_width_per_band;
                break;
        }
        
        float band_value = audio_data[i] / 255.0f;

        // Calculate bar height based on band value
        int bar_height = static_cast<int>(band_value * height);
        bar_height = std::min(bar_height, height);

        // Draw the bar
        for (int y = 0; y < bar_height; y++) {
            // Calculate color intensity based on height
            float intensity = static_cast<float>(height - y) / height;
            uint32_t color = get_bar_color(i, intensity, num_bands);

            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            // Draw the main bar
            for (int w = 0; w < bar_width->get(); w++) {
                offscreen_canvas->SetPixel(x + w, height - 1 - y, r, g, b);

                // Mirror if enabled, but only in normal mode
                if (mirror_display->get() && display_mode->get() == 0) {
                    offscreen_canvas->SetPixel(width - 1 - (x + w), height - 1 - y, r, g, b);
                }
            }
        }

        // Draw the peak dot
        if (falling_dots->get()) {
            int peak_y = static_cast<int>(peak_positions[i] * height);
            peak_y = std::min(peak_y, height - 1);

            uint32_t peak_color = get_bar_color(i, 1.0f, num_bands);
            uint8_t r = (peak_color >> 16) & 0xFF;
            uint8_t g = (peak_color >> 8) & 0xFF;
            uint8_t b = peak_color & 0xFF;

            for (int w = 0; w < bar_width->get(); w++) {
                offscreen_canvas->SetPixel(x + w, height - 1 - peak_y, r, g, b);

                // Mirror if enabled, but only in normal mode
                if (mirror_display->get() && display_mode->get() == 0) {
                    offscreen_canvas->SetPixel(width - 1 - (x + w), height - 1 - peak_y, r, g, b);
                }
            }
        }
    }

    return true;
}

void AudioSpectrumScene::render_circle_visualization(rgb_matrix::RGBMatrixBase *matrix, const std::vector<uint8_t> &audio_data) {
    const int width = matrix->width();
    const int height = matrix->height();
    const int center_x = width / 2;
    const int center_y = height / 2;
    const float max_radius = std::min(width, height) / 2.0f * circle_radius->get();
    
    const int num_bands = std::min(static_cast<int>(audio_data.size()), 64); // Limit for performance
    const float angle_step = 2 * M_PI / num_bands;
    
    initialize_if_needed(num_bands);
    
    // Update peak positions
    for (int i = 0; i < num_bands; i++) {
        float band_value = audio_data[i] / 255.0f;
        
        if (band_value > peak_positions[i]) {
            peak_positions[i] = band_value;
        } else if (falling_dots->get()) {
            peak_positions[i] -= dot_fall_speed->get() * 0.016f; // Assume 60 FPS
            peak_positions[i] = std::max(0.0f, peak_positions[i]);
        }
    }
    
    // Draw circular spectrum
    for (int i = 0; i < num_bands; i++) {
        float angle = i * angle_step + rotation_angle;
        float band_value = audio_data[i] / 255.0f;
        
        // Draw radial line from center outward
        float line_length = band_value * max_radius;
        int steps = static_cast<int>(line_length);
        
        for (int step = 0; step < steps; step++) {
            float radius = static_cast<float>(step);
            auto [x, y] = polar_to_cartesian(radius, angle, center_x, center_y);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                float intensity = 1.0f - (radius / max_radius) * 0.5f; // Fade towards outside
                uint32_t color = get_bar_color(i, intensity, num_bands);
                
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }
        
        // Draw peak dot
        if (falling_dots->get()) {
            float peak_radius = peak_positions[i] * max_radius;
            auto [peak_x, peak_y] = polar_to_cartesian(peak_radius, angle, center_x, center_y);
            
            if (peak_x >= 0 && peak_x < width && peak_y >= 0 && peak_y < height) {
                uint32_t peak_color = get_bar_color(i, 1.0f, num_bands);
                uint8_t r = (peak_color >> 16) & 0xFF;
                uint8_t g = (peak_color >> 8) & 0xFF;
                uint8_t b = peak_color & 0xFF;
                
                offscreen_canvas->SetPixel(peak_x, peak_y, r, g, b);
            }
        }
    }
}

void AudioSpectrumScene::render_spiral_visualization(rgb_matrix::RGBMatrixBase *matrix, const std::vector<uint8_t> &audio_data) {
    const int width = matrix->width();
    const int height = matrix->height();
    const int center_x = width / 2;
    const int center_y = height / 2;
    const float max_radius = std::min(width, height) / 2.0f * circle_radius->get();
    
    const int num_bands = std::min(static_cast<int>(audio_data.size()), 128);
    const float spiral_turns = 3.0f; // Number of spiral turns
    const float angle_step = (2 * M_PI * spiral_turns) / num_bands;
    
    initialize_if_needed(num_bands);
    
    // Update peak positions
    for (int i = 0; i < num_bands; i++) {
        float band_value = audio_data[i] / 255.0f;
        
        if (band_value > peak_positions[i]) {
            peak_positions[i] = band_value;
        } else if (falling_dots->get()) {
            peak_positions[i] -= dot_fall_speed->get() * 0.016f;
            peak_positions[i] = std::max(0.0f, peak_positions[i]);
        }
    }
    
    // Draw spiral spectrum
    for (int i = 0; i < num_bands; i++) {
        float angle = i * angle_step + rotation_angle;
        float base_radius = (static_cast<float>(i) / num_bands) * max_radius;
        float band_value = audio_data[i] / 255.0f;
        
        // Draw line outward from spiral path
        float line_length = band_value * max_radius * 0.3f; // Shorter lines for spiral
        int steps = static_cast<int>(line_length);
        
        for (int step = 0; step < steps; step++) {
            float radius = base_radius + static_cast<float>(step);
            auto [x, y] = polar_to_cartesian(radius, angle, center_x, center_y);
            
            if (x >= 0 && x < width && y >= 0 && y < height) {
                float intensity = 1.0f - (static_cast<float>(step) / line_length) * 0.3f;
                uint32_t color = get_bar_color(i, intensity, num_bands);
                
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }
        
        // Draw peak dot
        if (falling_dots->get()) {
            float peak_radius = base_radius + peak_positions[i] * max_radius * 0.3f;
            auto [peak_x, peak_y] = polar_to_cartesian(peak_radius, angle, center_x, center_y);
            
            if (peak_x >= 0 && peak_x < width && peak_y >= 0 && peak_y < height) {
                uint32_t peak_color = get_bar_color(i, 1.0f, num_bands);
                uint8_t r = (peak_color >> 16) & 0xFF;
                uint8_t g = (peak_color >> 8) & 0xFF;
                uint8_t b = peak_color & 0xFF;
                
                offscreen_canvas->SetPixel(peak_x, peak_y, r, g, b);
            }
        }
    }
}

std::pair<int, int> AudioSpectrumScene::polar_to_cartesian(float radius, float angle, int center_x, int center_y) const {
    int x = static_cast<int>(center_x + radius * cos(angle));
    int y = static_cast<int>(center_y + radius * sin(angle));
    return {x, y};
}

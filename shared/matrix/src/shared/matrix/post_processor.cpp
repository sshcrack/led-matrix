#include "shared/matrix/post_processor.h"
#include <algorithm>
#include <cmath>
#include "spdlog/spdlog.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void PostProcessor::add_effect(PostProcessType type, float duration, float intensity) {
    active_effects.emplace_back(type, duration, intensity);
    spdlog::debug("Added post-processing effect type: {}, duration: {:.2f}s", 
                 static_cast<int>(type), duration);
}

void PostProcessor::process_canvas(RGBMatrixBase* matrix, FrameCanvas* canvas) {
    if (active_effects.empty()) {
        return;
    }
    
    // Remove expired effects
    active_effects.erase(
        std::remove_if(active_effects.begin(), active_effects.end(),
                      [this](const PostProcessEffect& effect) {
                          return is_effect_expired(effect);
                      }),
        active_effects.end()
    );
    
    // Apply all active effects
    for (const auto& effect : active_effects) {
        switch (effect.type) {
            case PostProcessType::Flash:
                apply_flash_effect(canvas, effect);
                break;
            case PostProcessType::Rotate:
                apply_rotate_effect(canvas, effect);
                break;
        }
    }
}

void PostProcessor::clear_effects() {
    active_effects.clear();
}

bool PostProcessor::has_active_effects() const {
    return !active_effects.empty();
}

float PostProcessor::get_effect_progress(const PostProcessEffect& effect) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - effect.start_time);
    return std::min(1.0f, elapsed.count() / effect.duration_seconds);
}

bool PostProcessor::is_effect_expired(const PostProcessEffect& effect) {
    return get_effect_progress(effect) >= 1.0f;
}

void PostProcessor::apply_flash_effect(FrameCanvas* canvas, const PostProcessEffect& effect) {
    float progress = get_effect_progress(effect);
    
    // Create a flash that peaks quickly and fades out
    float flash_intensity;
    if (progress < 0.1f) {
        // Quick ramp up to peak
        flash_intensity = (progress / 0.1f) * effect.intensity;
    } else {
        // Exponential decay
        float decay_progress = (progress - 0.1f) / 0.9f;
        flash_intensity = effect.intensity * std::exp(-decay_progress * 5.0f);
    }
    
    int width = canvas->width();
    int height = canvas->height();
    
    // Apply flash by brightening all pixels
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Get current pixel color
            uint8_t r, g, b;
            canvas->GetPixel(x, y, &r, &g, &b);
            
            // Brighten the pixel based on flash intensity
            int new_r = std::min(255, static_cast<int>(r + flash_intensity * (255 - r) * 0.8f));
            int new_g = std::min(255, static_cast<int>(g + flash_intensity * (255 - g) * 0.8f));
            int new_b = std::min(255, static_cast<int>(b + flash_intensity * (255 - b) * 0.8f));
            
            canvas->SetPixel(x, y, new_r, new_g, new_b);
        }
    }
}

void PostProcessor::apply_rotate_effect(FrameCanvas* canvas, const PostProcessEffect& effect) {
    float progress = get_effect_progress(effect);
    
    // Rotate up to 360 degrees over the duration
    float rotation_degrees = progress * 360.0f * effect.intensity;
    float rotation_radians = rotation_degrees * M_PI / 180.0f;
    
    int width = canvas->width();
    int height = canvas->height();
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Create a temporary canvas for rotation
    std::vector<std::vector<rgb_matrix::Color>> temp_canvas(height, std::vector<rgb_matrix::Color>(width));
    
    // Copy current canvas to temp
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r, g, b;
            canvas->GetPixel(x, y, &r, &g, &b);
            temp_canvas[y][x] = rgb_matrix::Color(r, g, b);
        }
    }
    
    // Clear canvas for rotated image
    canvas->Clear();
    
    // Apply rotation
    float cos_angle = std::cos(rotation_radians);
    float sin_angle = std::sin(rotation_radians);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Translate to origin
            int rel_x = x - center_x;
            int rel_y = y - center_y;
            
            // Rotate
            int src_x = static_cast<int>(rel_x * cos_angle - rel_y * sin_angle + center_x);
            int src_y = static_cast<int>(rel_x * sin_angle + rel_y * cos_angle + center_y);
            
            // Check bounds and copy pixel
            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                const auto& color = temp_canvas[src_y][src_x];
                canvas->SetPixel(x, y, color.r, color.g, color.b);
            }
        }
    }
}
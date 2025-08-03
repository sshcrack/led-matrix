#include "BasicEffects.h"
#include "spdlog/spdlog.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declaration of global post processor
extern class PostProcessor* global_post_processor;

using namespace Plugins;

extern "C" PLUGIN_EXPORT BasicEffects *createBasicEffects() {
    return new BasicEffects();
}

extern "C" PLUGIN_EXPORT void destroyBasicEffects(BasicEffects *c) {
    delete c;
}

BasicEffects::BasicEffects() {
    spdlog::info("BasicEffects plugin initialized");
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> BasicEffects::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> BasicEffects::create_scenes() {
    return {};
}

std::optional<string> BasicEffects::before_server_init() {
    // Register our effects with the global post processor
    if (global_post_processor) {
        global_post_processor->register_effect(std::make_unique<FlashEffect>());
        global_post_processor->register_effect(std::make_unique<RotateEffect>());
        spdlog::info("Registered basic post-processing effects: flash, rotate");
    } else {
        spdlog::error("Global post processor not available - cannot register effects");
    }
    return std::nullopt;
}

std::string BasicEffects::get_plugin_name() const {
    return "BasicEffects";
}

// Flash effect implementation
void FlashEffect::apply(FrameCanvas* canvas, const PostProcessEffect& effect) {
    if (!canvas) return;
    
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
            
            // Skip completely black pixels to preserve intentional darkness
            if (r == 0 && g == 0 && b == 0) continue;
            
            // Brighten the pixel based on flash intensity
            int new_r = std::min(255, static_cast<int>(r + flash_intensity * (255 - r) * 0.8f));
            int new_g = std::min(255, static_cast<int>(g + flash_intensity * (255 - g) * 0.8f));
            int new_b = std::min(255, static_cast<int>(b + flash_intensity * (255 - b) * 0.8f));
            
            canvas->SetPixel(x, y, new_r, new_g, new_b);
        }
    }
}

// Rotate effect implementation
void RotateEffect::apply(FrameCanvas* canvas, const PostProcessEffect& effect) {
    if (!canvas) return;
    
    float progress = get_effect_progress(effect);
    
    // Rotate up to 360 degrees over the duration
    float rotation_degrees = progress * 360.0f * effect.intensity;
    float rotation_radians = rotation_degrees * M_PI / 180.0f;
    
    int width = canvas->width();
    int height = canvas->height();
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Only apply rotation if the canvas is not empty and rotation is significant
    if (std::abs(rotation_degrees) < 1.0f) return;
    
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
    
    // Apply rotation with improved sampling
    float cos_angle = std::cos(rotation_radians);
    float sin_angle = std::sin(rotation_radians);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Translate to origin
            int rel_x = x - center_x;
            int rel_y = y - center_y;
            
            // Rotate (inverse transformation)
            int src_x = static_cast<int>(rel_x * cos_angle + rel_y * sin_angle + center_x);
            int src_y = static_cast<int>(-rel_x * sin_angle + rel_y * cos_angle + center_y);
            
            // Check bounds and copy pixel
            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                const auto& color = temp_canvas[src_y][src_x];
                canvas->SetPixel(x, y, color.r, color.g, color.b);
            }
        }
    }
}
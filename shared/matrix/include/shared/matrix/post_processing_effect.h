#pragma once

#include "led-matrix.h"
#include <string>
#include <memory>
#include <chrono>
#include <functional>

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

struct PostProcessEffect {
    std::string effect_name;
    std::chrono::steady_clock::time_point start_time;
    float duration_seconds;
    float intensity; // 0.0 to 1.0+
    
    PostProcessEffect(const std::string& name, float duration = 0.5f, float intensity = 1.0f) 
        : effect_name(name), start_time(std::chrono::steady_clock::now()), 
          duration_seconds(duration), intensity(intensity) {}
};

class PostProcessingEffect {
public:
    virtual ~PostProcessingEffect() = default;
    
    // Get the name of this effect
    virtual std::string get_name() const = 0;
    
    // Apply the effect to the canvas
    virtual void apply(FrameCanvas* canvas, const PostProcessEffect& effect) = 0;
    
    // Helper function to calculate effect progress (0.0 to 1.0)
    static float get_effect_progress(const PostProcessEffect& effect) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - effect.start_time);
        return std::min(1.0f, elapsed.count() / effect.duration_seconds);
    }
    
    // Helper function to check if effect is expired
    static bool is_effect_expired(const PostProcessEffect& effect) {
        return get_effect_progress(effect) >= 1.0f;
    }
};
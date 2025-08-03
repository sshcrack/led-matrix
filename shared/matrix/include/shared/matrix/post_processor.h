#pragma once

#include "led-matrix.h"
#include <memory>
#include <vector>
#include <chrono>
#include <functional>

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

enum class PostProcessType {
    Flash,
    Rotate
};

struct PostProcessEffect {
    PostProcessType type;
    std::chrono::steady_clock::time_point start_time;
    float duration_seconds;
    float intensity; // 0.0 to 1.0
    
    PostProcessEffect(PostProcessType t, float duration = 0.5f, float intensity = 1.0f) 
        : type(t), start_time(std::chrono::steady_clock::now()), 
          duration_seconds(duration), intensity(intensity) {}
};

class PostProcessor {
private:
    std::vector<PostProcessEffect> active_effects;
    
    // Post-processing implementations
    void apply_flash_effect(FrameCanvas* canvas, const PostProcessEffect& effect);
    void apply_rotate_effect(FrameCanvas* canvas, const PostProcessEffect& effect);
    
    // Helper functions
    float get_effect_progress(const PostProcessEffect& effect);
    bool is_effect_expired(const PostProcessEffect& effect);

public:
    PostProcessor() = default;
    ~PostProcessor() = default;
    
    // Add a new post-processing effect
    void add_effect(PostProcessType type, float duration = 0.5f, float intensity = 1.0f);
    
    // Apply all active post-processing effects to the canvas
    void process_canvas(RGBMatrixBase* matrix, FrameCanvas* canvas);
    
    // Clear all effects
    void clear_effects();
    
    // Check if any effects are active
    bool has_active_effects() const;
};
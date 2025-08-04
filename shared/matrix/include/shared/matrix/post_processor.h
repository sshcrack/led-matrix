#pragma once

#include "led-matrix.h"
#include "post_processing_effect.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class PostProcessor {
private:
    std::vector<PostProcessEffect> active_effects;
    std::unordered_map<std::string, std::unique_ptr<PostProcessingEffect, void (*)(PostProcessingEffect *)>> registered_effects;

public:
    PostProcessor() = default;
    ~PostProcessor() = default;
    
    // Register a new post-processing effect type
    void register_effect(std::unique_ptr<PostProcessingEffect, void (*)(PostProcessingEffect *)> effect);
    
    // Add a new post-processing effect by name
    bool add_effect(const std::string& effect_name, float duration = 0.5f, float intensity = 1.0f);
    
    // Apply all active post-processing effects to the canvas
    void process_canvas(RGBMatrixBase* matrix, FrameCanvas* canvas);
    
    // Clear all effects
    void clear_effects();
    
    // Check if any effects are active
    bool has_active_effects() const;
    
    // Get list of registered effect names
    std::vector<std::string> get_registered_effects() const;
};
#include "shared/matrix/post_processor.h"
#include <algorithm>
#include "spdlog/spdlog.h"

void PostProcessor::register_effect(std::unique_ptr<PostProcessingEffect, void (*)(PostProcessingEffect *)> effect) {
    if (effect) {
        std::string name = effect->get_name();
        registered_effects.emplace(std::move(name), std::move(effect));
        spdlog::debug("Registered post-processing effect: {}", name);
    }
}

bool PostProcessor::add_effect(const std::string& effect_name, float duration, float intensity) {
    auto it = registered_effects.find(effect_name);
    if (it == registered_effects.end()) {
        spdlog::warn("Unknown post-processing effect: {}", effect_name);
        return false;
    }
    
    active_effects.emplace_back(effect_name, duration, intensity);
    spdlog::debug("Added post-processing effect: {}, duration: {:.2f}s", effect_name, duration);
    return true;
}

void PostProcessor::process_canvas(RGBMatrixBase* matrix, FrameCanvas* canvas) {
    if (active_effects.empty() || !canvas) {
        return;
    }
    
    // Remove expired effects
    active_effects.erase(
        std::remove_if(active_effects.begin(), active_effects.end(),
                      [](const PostProcessEffect& effect) {
                          return PostProcessingEffect::is_effect_expired(effect);
                      }),
        active_effects.end()
    );
    
    // Apply all active effects
    for (const auto& effect : active_effects) {
        auto it = registered_effects.find(effect.effect_name);
        if (it != registered_effects.end()) {
            it->second->apply(canvas, effect);
        }
    }
}

void PostProcessor::clear_effects() {
    active_effects.clear();
}

bool PostProcessor::has_active_effects() const {
    return !active_effects.empty();
}

std::vector<std::string> PostProcessor::get_registered_effects() const {
    std::vector<std::string> names;
    names.reserve(registered_effects.size());
    for (const auto& pair : registered_effects) {
        names.push_back(pair.first);
    }
    return names;
}
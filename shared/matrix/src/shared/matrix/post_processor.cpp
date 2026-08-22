#include "shared/matrix/post_processor.h"

#include <algorithm>

#include "spdlog/spdlog.h"

void PostProcessor::register_effect(std::unique_ptr<PostProcessingEffect> effect)
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    if (effect) {
        std::string name = effect->get_name();
        spdlog::debug("Registered post-processing effect: {}", name);
        registered_effects.emplace(name, std::move(effect));
    }
}

bool PostProcessor::add_effect(const std::string& effect_name, float duration, float intensity)
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    auto it = registered_effects.find(effect_name);
    if (it == registered_effects.end()) {
        spdlog::warn("Unknown post-processing effect: {}", effect_name);
        return false;
    }

    // Do not stack identical accents. Repeated audio/UI triggers used to apply
    // the same luminance/distortion operation several times in one frame, which
    // could look like flicker even when each individual effect was restrained.
    const auto active = std::find_if(active_effects.begin(), active_effects.end(), [&](const PostProcessEffect& effect) {
        return effect.effect_name == effect_name && !PostProcessingEffect::is_effect_expired(effect);
    });
    if (active != active_effects.end()) {
        active->intensity = std::max(active->intensity, intensity);
        active->duration_seconds = std::max(active->duration_seconds, duration);
        return true;
    }

    active_effects.emplace_back(effect_name, duration, intensity);
    spdlog::debug("Added post-processing effect: {}, duration: {:.2f}s", effect_name, duration);
    return true;
}

void PostProcessor::apply_effects(FrameCanvas* canvas)
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    if (active_effects.empty() || !canvas) {
        return;
    }

    // Remove expired effects
    active_effects.erase(std::remove_if(active_effects.begin(), active_effects.end(),
                                        [](const PostProcessEffect& effect) { return PostProcessingEffect::is_effect_expired(effect); }),
                         active_effects.end());

    // Apply all active effects
    for (const auto& effect : active_effects) {
        auto it = registered_effects.find(effect.effect_name);
        if (it != registered_effects.end()) {
            it->second->apply(canvas, effect);
        }
    }
}

void PostProcessor::clear_effects()
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    active_effects.clear();
}

bool PostProcessor::has_active_effects() const
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    return !active_effects.empty();
}

std::vector<std::string> PostProcessor::get_registered_effects() const
{
    std::lock_guard<std::mutex> lock(effectsMutex);
    std::vector<std::string> names;
    names.reserve(registered_effects.size());
    for (const auto& pair : registered_effects) {
        names.push_back(pair.first);
    }
    return names;
}
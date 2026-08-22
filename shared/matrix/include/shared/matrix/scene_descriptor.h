#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace Scenes {

/// A curated look for one scene. `properties` contains only overrides from the
/// scene defaults; explicit user arguments are applied afterwards and therefore
/// always win. Optional profile values let a director reason about a variant
/// without knowing its scene-specific property names.
struct SceneVariant {
    std::string id;
    std::string label;
    std::string description;
    nlohmann::json properties = nlohmann::json::object();
    std::vector<std::string> tags;
    std::optional<float> intensity;
    std::optional<float> motion;
    std::optional<float> music_affinity;
    std::optional<float> performance_cost;
};

/// Product-level metadata used by Automatic Mode, Scene Lab and the web UI.
/// Every numeric value is normalized to 0..1. Scene-specific knobs stay in the
/// property system; directors consume this small stable interface instead.
struct SceneDescriptor {
    std::string family;
    std::vector<std::string> tags;
    float intensity = 0.5f;
    float motion = 0.5f;
    float music_affinity = 0.0f;
    float performance_cost = 0.5f;
    bool automatic_eligible = true;
    std::vector<SceneVariant> variants;
};

struct EffectiveSceneProfile {
    float intensity = 0.5f;
    float motion = 0.5f;
    float music_affinity = 0.0f;
    float performance_cost = 0.5f;
    std::vector<std::string> tags;
};

[[nodiscard]] EffectiveSceneProfile effective_profile(
    const SceneDescriptor &descriptor, const SceneVariant *variant = nullptr);
[[nodiscard]] const SceneVariant *find_variant(
    const SceneDescriptor &descriptor, std::string_view id);
[[nodiscard]] nlohmann::json descriptor_to_json(const SceneDescriptor &descriptor);

} // namespace Scenes

#include <shared/matrix/scene_descriptor.h>

#include <algorithm>

namespace Scenes {
namespace {
float normalized(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}
}

EffectiveSceneProfile effective_profile(
    const SceneDescriptor &descriptor, const SceneVariant *variant)
{
    EffectiveSceneProfile result;
    result.intensity = normalized(variant && variant->intensity.has_value()
        ? *variant->intensity : descriptor.intensity);
    result.motion = normalized(variant && variant->motion.has_value()
        ? *variant->motion : descriptor.motion);
    result.music_affinity = normalized(variant && variant->music_affinity.has_value()
        ? *variant->music_affinity : descriptor.music_affinity);
    result.performance_cost = normalized(variant && variant->performance_cost.has_value()
        ? *variant->performance_cost : descriptor.performance_cost);
    result.tags = descriptor.tags;
    if (variant) {
        for (const auto &tag : variant->tags) {
            if (std::find(result.tags.begin(), result.tags.end(), tag) == result.tags.end())
                result.tags.push_back(tag);
        }
    }
    return result;
}

const SceneVariant *find_variant(const SceneDescriptor &descriptor, std::string_view id)
{
    const auto it = std::find_if(descriptor.variants.begin(), descriptor.variants.end(),
        [id](const SceneVariant &variant) { return variant.id == id; });
    return it == descriptor.variants.end() ? nullptr : &*it;
}

nlohmann::json descriptor_to_json(const SceneDescriptor &descriptor)
{
    nlohmann::json variants = nlohmann::json::array();
    for (const auto &variant : descriptor.variants) {
        nlohmann::json item{
            {"id", variant.id},
            {"label", variant.label},
            {"description", variant.description},
            {"properties", variant.properties},
            {"tags", variant.tags},
        };
        if (variant.intensity.has_value()) item["intensity"] = *variant.intensity;
        if (variant.motion.has_value()) item["motion"] = *variant.motion;
        if (variant.music_affinity.has_value()) item["music_affinity"] = *variant.music_affinity;
        if (variant.performance_cost.has_value()) item["performance_cost"] = *variant.performance_cost;
        item["automatic_default"] = variant.automatic_default;
        variants.push_back(std::move(item));
    }

    return {
        {"family", descriptor.family},
        {"tags", descriptor.tags},
        {"intensity", descriptor.intensity},
        {"motion", descriptor.motion},
        {"music_affinity", descriptor.music_affinity},
        {"performance_cost", descriptor.performance_cost},
        {"automatic_eligible", descriptor.automatic_eligible},
        {"automatic_default", descriptor.automatic_default},
        {"variants", std::move(variants)},
    };
}

bool automatic_default(const SceneDescriptor &descriptor, const SceneVariant *variant)
{
    return descriptor.automatic_default && (variant == nullptr || variant->automatic_default);
}

void set_off_by_default(SceneDescriptor &descriptor, std::initializer_list<std::string_view> variant_ids)
{
    for (auto &variant : descriptor.variants) {
        if (std::find(variant_ids.begin(), variant_ids.end(), variant.id) != variant_ids.end())
            variant.automatic_default = false;
    }
}

} // namespace Scenes

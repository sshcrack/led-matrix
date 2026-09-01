#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Scenes {

struct CustomShaderMetadata {
    std::string family = "shader";
    std::vector<std::string> tags{"shader", "shadertoy", "scenic"};
    float intensity = 0.55f;
    float motion = 0.55f;
    float music_affinity = 0.15f;
    float performance_cost = 0.55f;
    bool automatic_eligible = false;
    bool audio_reactive = false;
};

/// Reads an optional `/* led-matrix-shader { ... } */` block near the top of
/// a .frag file. Unknown/missing fields retain conservative defaults so a
/// shader authored by an agent only needs to describe what is meaningful.
CustomShaderMetadata read_custom_shader_metadata(const std::filesystem::path &path);

} // namespace Scenes

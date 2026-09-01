#include "ShaderMetadata.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Scenes {
namespace {
constexpr std::string_view Marker = "/* led-matrix-shader";
constexpr std::size_t MaxHeaderBytes = 16 * 1024;

float normalized(const nlohmann::json &value, const char *field, float fallback) {
    if (!value.contains(field) || !value[field].is_number()) return fallback;
    return std::clamp(value[field].get<float>(), 0.0f, 1.0f);
}
}

CustomShaderMetadata read_custom_shader_metadata(const std::filesystem::path &path) {
    CustomShaderMetadata metadata;
    std::ifstream input(path, std::ios::binary);
    if (!input) return metadata;

    std::string header(MaxHeaderBytes, '\0');
    input.read(header.data(), static_cast<std::streamsize>(header.size()));
    header.resize(static_cast<std::size_t>(input.gcount()));

    const auto marker = header.find(Marker);
    if (marker == std::string::npos) return metadata;
    const auto end = header.find("*/", marker + Marker.size());
    if (end == std::string::npos) {
        spdlog::warn("Shadertoy: unterminated led-matrix-shader metadata in '{}'", path.string());
        return metadata;
    }

    const auto objectStart = header.find('{', marker + Marker.size());
    if (objectStart == std::string::npos || objectStart >= end) return metadata;

    try {
        const auto parsed = nlohmann::json::parse(header.substr(objectStart, end - objectStart));
        if (const auto it = parsed.find("family"); it != parsed.end() && it->is_string())
            metadata.family = it->get<std::string>();
        if (const auto it = parsed.find("tags"); it != parsed.end() && it->is_array()) {
            metadata.tags.clear();
            for (const auto &tag : *it)
                if (tag.is_string()) metadata.tags.push_back(tag.get<std::string>());
        }
        metadata.intensity = normalized(parsed, "intensity", metadata.intensity);
        metadata.motion = normalized(parsed, "motion", metadata.motion);
        metadata.music_affinity = normalized(parsed, "music_affinity", metadata.music_affinity);
        metadata.performance_cost = normalized(parsed, "performance_cost", metadata.performance_cost);
        metadata.automatic_eligible = parsed.value("automatic_eligible", metadata.automatic_eligible);
        metadata.audio_reactive = parsed.value("audio_reactive", metadata.audio_reactive);
        if (metadata.audio_reactive) {
            if (std::find(metadata.tags.begin(), metadata.tags.end(), "audio-reactive") == metadata.tags.end())
                metadata.tags.emplace_back("audio-reactive");
            if (std::find(metadata.tags.begin(), metadata.tags.end(), "music") == metadata.tags.end())
                metadata.tags.emplace_back("music");
        }
    } catch (const std::exception &error) {
        spdlog::warn("Shadertoy: invalid led-matrix-shader metadata in '{}': {}", path.string(), error.what());
    }
    return metadata;
}

} // namespace Scenes

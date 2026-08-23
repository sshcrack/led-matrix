#pragma once

#include <cstdint>
#include <vector>

namespace AmbientScenes {

struct MetaballParams {
    int width = 128;
    int height = 128;
    int blob_count = 8;
    float speed = 0.25f;
    float move_range = 0.5f;
    float color_speed = 0.03f;
    float threshold = 0.0003f;
    bool audio_reactive = false;
    float audio_strength = 1.0f;
};

struct MetaballAudio {
    float bass = 0.0f;
    float mids = 0.0f;
    float treble = 0.0f;
    float balance = 0.0f;
    float beat_pulse = 0.0f;
    float drop_pulse = 0.0f;
    float section_hue = 0.0f;
};

/// CPU-friendly software metaball renderer shared by the Pi scene and the
/// desktop offload worker. The renderer automatically lowers only the scalar
/// field sampling resolution under pressure, then bilinearly reconstructs the
/// final 128x128 frame so quality degradation is smooth instead of blocky.
class MetaballRenderer {
public:
    const std::vector<std::uint8_t> &render(
        const MetaballParams &params,
        const MetaballAudio &audio,
        float time_seconds,
        float quality_scale = 1.0f);

private:
    std::vector<std::uint8_t> samples_;
    std::vector<std::uint8_t> frame_;
};

} // namespace AmbientScenes

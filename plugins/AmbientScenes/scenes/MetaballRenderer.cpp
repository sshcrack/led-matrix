#include "MetaballRenderer.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace AmbientScenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;

float smoothstep(float edge0, float edge1, float value)
{
    if (edge0 == edge1)
        return value < edge0 ? 0.0f : 1.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

std::tuple<float, float, float> hue_to_rgb(float hue)
{
    hue -= std::floor(hue);
    const float h = hue * 6.0f;
    const int sector = static_cast<int>(h) % 6;
    const float f = h - std::floor(h);
    const float q = 1.0f - f;

    switch (sector) {
        case 0: return {1.0f, f, 0.0f};
        case 1: return {q, 1.0f, 0.0f};
        case 2: return {0.0f, 1.0f, f};
        case 3: return {0.0f, q, 1.0f};
        case 4: return {f, 0.0f, 1.0f};
        default: return {1.0f, 0.0f, q};
    }
}

struct Blob {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 1.0f;
    float radius_squared = 1.0f;
};

int sampling_step(float quality_scale)
{
    // Full quality remains available on fast hardware. A two-pixel scalar
    // field cuts the expensive O(width*height*blob_count) work by ~4x while
    // bilinear reconstruction keeps the 128x128 output visually smooth.
    if (quality_scale >= 0.90f)
        return 1;
    if (quality_scale >= 0.62f)
        return 2;
    return 3;
}

std::uint8_t byte_from_unit(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value * 255.0f, 0.0f, 255.0f));
}
} // namespace

const std::vector<std::uint8_t> &MetaballRenderer::render(
    const MetaballParams &params,
    const MetaballAudio &audio,
    float time_seconds,
    float quality_scale)
{
    const int width = std::max(1, params.width);
    const int height = std::max(1, params.height);
    const int blob_count = std::max(1, params.blob_count);
    const int step = sampling_step(std::clamp(quality_scale, 0.45f, 1.0f));

    std::vector<Blob> blobs;
    blobs.reserve(static_cast<std::size_t>(blob_count));
    const float min_dimension = static_cast<float>(std::min(width, height));
    const float speed_scale_base = params.speed *
        (1.0f + (params.audio_reactive ? audio.mids * params.audio_strength * 0.9f : 0.0f));

    for (int i = 0; i < blob_count; ++i) {
        const float phase = static_cast<float>(i) * 1.731f;
        const float speed_scale = speed_scale_base;
        float x = 0.5f
            + params.move_range * 0.42f * std::sin(time_seconds * speed_scale * (0.72f + 0.07f * (i % 5)) + phase)
            + params.move_range * 0.11f * std::sin(time_seconds * speed_scale * 1.83f + phase * 0.37f);
        float y = 0.5f
            + params.move_range * 0.38f * std::cos(time_seconds * speed_scale * (0.61f + 0.05f * (i % 7)) + phase * 0.83f)
            + params.move_range * 0.13f * std::sin(time_seconds * speed_scale * 1.37f + phase * 0.51f);
        if (params.audio_reactive)
            x += audio.balance * params.audio_strength * 0.10f;
        x = std::clamp(x, -0.15f, 1.15f);
        y = std::clamp(y, -0.15f, 1.15f);

        const float audio_radius = params.audio_reactive
            ? (audio.bass * 0.30f + audio.beat_pulse * 0.16f + audio.drop_pulse * 0.34f) * params.audio_strength
            : 0.0f;
        const float radius_wave = 0.88f + audio_radius
            + 0.20f * std::sin(time_seconds * speed_scale * 0.9f + phase);
        const float radius = std::max(
            2.0f,
            min_dimension * (0.075f + 0.022f * static_cast<float>(i % 4)) * radius_wave);
        blobs.push_back({
            x * static_cast<float>(width),
            y * static_cast<float>(height),
            radius,
            radius * radius,
        });
    }

    const int sample_width = (width - 1) / step + 1;
    const int sample_height = (height - 1) / step + 1;
    samples_.assign(static_cast<std::size_t>(sample_width * sample_height * 3), 0);

    const float user_threshold = std::max(0.000001f, params.threshold);
    const float surface = std::clamp(user_threshold / 0.0003f, 0.15f, 4.0f) * 1.18f;
    const float base_hue = std::fmod(
        time_seconds * params.color_speed + audio.section_hue + audio.drop_pulse * 0.06f,
        1.0f);
    const float inv_width = 1.0f / static_cast<float>(width);
    const float inv_height = 1.0f / static_cast<float>(height);
    const float rim_width = 0.22f +
        (params.audio_reactive ? audio.treble * params.audio_strength * 0.12f : 0.0f);

    for (int sy = 0; sy < sample_height; ++sy) {
        const int y = std::min(height - 1, sy * step);
        for (int sx = 0; sx < sample_width; ++sx) {
            const int x = std::min(width - 1, sx * step);
            float field = 0.0f;
            float nearest_ratio_squared = 1.0e9f;

            for (const auto &blob : blobs) {
                const float dx = static_cast<float>(x) - blob.x;
                const float dy = static_cast<float>(y) - blob.y;
                const float raw_distance_squared = dx * dx + dy * dy;
                const float field_distance_squared = raw_distance_squared + 1.0f;
                field += blob.radius_squared / field_distance_squared;
                nearest_ratio_squared = std::min(
                    nearest_ratio_squared,
                    raw_distance_squared / blob.radius_squared);
            }

            const float body = smoothstep(surface * 0.76f, surface * 1.20f, field);
            const float aura = smoothstep(surface * 0.18f, surface * 0.82f, field) * (1.0f - body * 0.72f);
            const float rim_distance = std::abs(field - surface) / surface;
            const float rim = 1.0f - smoothstep(0.0f, rim_width, rim_distance);
            if (body <= 0.001f && rim <= 0.001f && aura <= 0.001f)
                continue;

            const float nx = static_cast<float>(x) * inv_width;
            const float ny = static_cast<float>(y) * inv_height;
            const float hue = base_hue
                + 0.10f * std::sin(nx * 2.0f * Pi + time_seconds * 0.35f)
                + 0.08f * std::cos(ny * 2.0f * Pi - time_seconds * 0.27f)
                + 0.035f * field;
            const auto [base_r, base_g, base_b] = hue_to_rgb(hue);
            const auto [rim_r, rim_g, rim_b] = hue_to_rgb(hue + 0.18f);

            // The old implementation took sqrt() once per blob per pixel. We
            // retain the same visual metric while taking it only once per
            // scalar-field sample after the nearest blob has been identified.
            const float nearest = std::sqrt(std::max(0.0f, nearest_ratio_squared));
            const float inner_glow = std::clamp((1.20f - nearest) * 0.32f, 0.0f, 0.30f);
            const float brightness = std::clamp(
                body * 0.76f + rim * 0.62f + aura * 0.075f + inner_glow,
                0.0f,
                1.0f);
            const float rim_mix = std::clamp(rim * 0.82f, 0.0f, 1.0f);

            const float mixed_r = base_r * (1.0f - rim_mix) + rim_r * rim_mix;
            const float mixed_g = base_g * (1.0f - rim_mix) + rim_g * rim_mix;
            const float mixed_b = base_b * (1.0f - rim_mix) + rim_b * rim_mix;
            const std::size_t index = static_cast<std::size_t>((sy * sample_width + sx) * 3);
            samples_[index + 0] = byte_from_unit(mixed_r * brightness);
            samples_[index + 1] = byte_from_unit(mixed_g * brightness);
            samples_[index + 2] = byte_from_unit(mixed_b * brightness);
        }
    }

    if (step == 1) {
        frame_ = samples_;
        return frame_;
    }

    frame_.resize(static_cast<std::size_t>(width * height * 3));
    for (int y = 0; y < height; ++y) {
        const float gy = static_cast<float>(y) / static_cast<float>(step);
        const int y0 = std::min(sample_height - 1, static_cast<int>(gy));
        const int y1 = std::min(sample_height - 1, y0 + 1);
        const float fy = std::clamp(gy - static_cast<float>(y0), 0.0f, 1.0f);
        for (int x = 0; x < width; ++x) {
            const float gx = static_cast<float>(x) / static_cast<float>(step);
            const int x0 = std::min(sample_width - 1, static_cast<int>(gx));
            const int x1 = std::min(sample_width - 1, x0 + 1);
            const float fx = std::clamp(gx - static_cast<float>(x0), 0.0f, 1.0f);

            const std::size_t i00 = static_cast<std::size_t>((y0 * sample_width + x0) * 3);
            const std::size_t i10 = static_cast<std::size_t>((y0 * sample_width + x1) * 3);
            const std::size_t i01 = static_cast<std::size_t>((y1 * sample_width + x0) * 3);
            const std::size_t i11 = static_cast<std::size_t>((y1 * sample_width + x1) * 3);
            const std::size_t dst = static_cast<std::size_t>((y * width + x) * 3);

            for (int channel = 0; channel < 3; ++channel) {
                const float top = static_cast<float>(samples_[i00 + channel]) * (1.0f - fx)
                    + static_cast<float>(samples_[i10 + channel]) * fx;
                const float bottom = static_cast<float>(samples_[i01 + channel]) * (1.0f - fx)
                    + static_cast<float>(samples_[i11 + channel]) * fx;
                frame_[dst + channel] = static_cast<std::uint8_t>(std::clamp(
                    top * (1.0f - fy) + bottom * fy,
                    0.0f,
                    255.0f));
            }
        }
    }

    return frame_;
}

} // namespace AmbientScenes

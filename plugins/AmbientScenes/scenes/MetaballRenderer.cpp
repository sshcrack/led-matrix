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
        float x = 0.5f + params.move_range * 0.68f * std::sin(time_seconds * speed_scale * (0.72f + 0.07f * (i % 5)) + phase) +
                  params.move_range * 0.11f * std::sin(time_seconds * speed_scale * 1.83f + phase * 0.37f);
        float y = 0.5f + params.move_range * 0.64f * std::cos(time_seconds * speed_scale * (0.61f + 0.05f * (i % 7)) + phase * 0.83f) +
                  params.move_range * 0.13f * std::sin(time_seconds * speed_scale * 1.37f + phase * 0.51f);
        if (params.audio_reactive)
            x += audio.balance * params.audio_strength * 0.10f;
        // Keep blobs mostly on the panel so large ones are not clipped for long; the whole
        // group wanders slowly so a joined mass does not sit in one footprint.
        x = std::clamp(0.5f + (x - 0.5f) * 0.72f + 0.10f * std::sin(time_seconds * 0.071f), 0.15f, 0.85f);
        y = std::clamp(0.5f + (y - 0.5f) * 0.72f + 0.08f * std::cos(time_seconds * 0.053f), 0.15f, 0.85f);

        const float audio_radius = params.audio_reactive
            ? (audio.bass * 0.30f + audio.beat_pulse * 0.16f + audio.drop_pulse * 0.34f) * params.audio_strength
            : 0.0f;
        const float radius_wave = 0.88f + audio_radius
            + 0.20f * std::sin(time_seconds * speed_scale * 0.9f + phase);
        // Many blobs share the panel: shrink each so merged masses stay readable.
        const float crowd = std::min(1.0f, std::sqrt(10.0f / static_cast<float>(blob_count)));
        const float radius = std::max(2.0f, crowd * min_dimension * (0.058f + 0.020f * static_cast<float>(i % 4)) * radius_wave);
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

    // Light from the upper left; the field's gradient gives each blob a rounded, glossy surface.
    const float lx = -0.45f, ly = -0.62f, lz = 0.64f;
    const float hx = lx, hy = ly, hz = lz + 1.0f;  // Blinn half vector towards a viewer straight ahead
    const float h_len = std::sqrt(hx * hx + hy * hy + hz * hz);
    const float spec_power = 30.0f + (params.audio_reactive ? audio.treble * params.audio_strength * 20.0f : 0.0f);

    for (int sy = 0; sy < sample_height; ++sy) {
        const int y = std::min(height - 1, sy * step);
        for (int sx = 0; sx < sample_width; ++sx) {
            const int x = std::min(width - 1, sx * step);
            float field = 0.0f, smooth = 0.0f, grad_x = 0.0f, grad_y = 0.0f;

            for (const auto &blob : blobs) {
                const float dx = static_cast<float>(x) - blob.x;
                const float dy = static_cast<float>(y) - blob.y;
                const float inv = 1.0f / (dx * dx + dy * dy + 1.0f);
                const float contribution = blob.radius_squared * inv;
                field += contribution;
                // Shading uses a much softer copy of the field: it gives a merged mass one rounded
                // surface instead of a dimple around every blob it is made of.
                const float soft_inv = 1.0f / (dx * dx + dy * dy + blob.radius_squared * 1.4f + 1.0f);
                const float soft = blob.radius_squared * soft_inv;
                smooth += soft;
                grad_x -= 2.0f * soft * soft_inv * dx;
                grad_y -= 2.0f * soft * soft_inv * dy;
            }

            const float body = smoothstep(surface * 0.90f, surface * 1.10f, field);
            const float aura = smoothstep(surface * 0.30f, surface * 0.95f, field) * (1.0f - body);
            const float rim_distance = std::abs(field - surface) / surface;
            const float rim = (1.0f - smoothstep(0.0f, rim_width, rim_distance)) * body;
            if (body <= 0.001f && aura <= 0.001f)
                continue;

            const float nx = static_cast<float>(x) * inv_width;
            const float ny = static_cast<float>(y) * inv_height;
            // Colour drifts across the panel but never cycles through the field, so blobs read as one liquid.
            const float hue = base_hue + 0.06f * std::sin(nx * 2.0f * Pi + time_seconds * 0.35f) +
                              0.05f * std::cos(ny * 2.0f * Pi - time_seconds * 0.27f);
            const auto [core_r, core_g, core_b] = hue_to_rgb(hue);
            const auto [edge_r, edge_g, edge_b] = hue_to_rgb(hue + 0.07f);

            // Surface normal from the slope of log(smooth field): flat on top, curving down at the rim.
            const float scale = 0.085f * min_dimension / std::max(smooth, 0.0001f);
            float n_x = -grad_x * scale, n_y = -grad_y * scale, n_z = 1.0f;
            const float n_len = std::sqrt(n_x * n_x + n_y * n_y + n_z * n_z);
            n_x /= n_len;
            n_y /= n_len;
            n_z /= n_len;
            const float diffuse = std::max(0.0f, n_x * lx + n_y * ly + n_z * lz);
            const float spec = std::pow(std::max(0.0f, (n_x * hx + n_y * hy + n_z * hz) / h_len), spec_power);

            // Capped, so the core colour is one smooth fill rather than a ring around every blob centre.
            const float depth = std::clamp(1.0f - surface / std::clamp(field, 0.0001f, surface * 2.2f), 0.0f, 1.0f);
            const float light = 0.28f + 0.72f * diffuse;
            const float pale = 0.25f * depth;  // centres are paler, like thick wax
            const auto shade = [&](float core, float edge) {
                const float base = edge + (core - edge) * depth;
                const float lit = (base + (1.0f - base) * pale) * light;
                // Small, lone blobs never reach a high field and stay dimmer than the main masses.
                const float mass = 0.72f + 0.28f * smoothstep(surface * 1.2f, surface * 3.5f, field);
                return (lit * body * 0.92f + std::min(spec, 0.6f) * body * 0.5f + rim * 0.18f * edge) * mass + aura * 0.16f * edge;
            };
            const std::size_t index = static_cast<std::size_t>((sy * sample_width + sx) * 3);
            samples_[index + 0] = byte_from_unit(shade(core_r, edge_r));
            samples_[index + 1] = byte_from_unit(shade(core_g, edge_g));
            samples_[index + 2] = byte_from_unit(shade(core_b, edge_b));
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

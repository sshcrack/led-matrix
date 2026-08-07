#include "MetaBlobScene.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <shared/matrix/audio_state.h>

namespace AmbientScenes {
    namespace {
        constexpr float PI = 3.14159265358979323846f;

        float smoothstep(float edge0, float edge1, float value) {
            if (edge0 == edge1) return value < edge0 ? 0.0f : 1.0f;
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        std::tuple<uint8_t, uint8_t, uint8_t> hue_to_rgb(float hue) {
            hue -= std::floor(hue);
            const float h = hue * 6.0f;
            const int sector = static_cast<int>(h) % 6;
            const float f = h - std::floor(h);
            const float q = 1.0f - f;

            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            switch (sector) {
                case 0: r = 1.0f; g = f; break;
                case 1: r = q; g = 1.0f; break;
                case 2: g = 1.0f; b = f; break;
                case 3: g = q; b = 1.0f; break;
                case 4: r = f; b = 1.0f; break;
                default: r = 1.0f; b = q; break;
            }

            return {
                static_cast<uint8_t>(r * 255.0f),
                static_cast<uint8_t>(g * 255.0f),
                static_cast<uint8_t>(b * 255.0f)
            };
        }
    }

    float MetaBlobScene::rand_sin(int i) const {
        return std::sin(static_cast<float>(i) * 1.64f);
    }

    MetaBlobScene::Blob MetaBlobScene::get_blob(int i, float current_time) const {
        const float phase = static_cast<float>(i) * 1.731f;
        const float speed_scale = speed->get() * (1.0f + (audio_reactive->get() ? audio_mids * audio_strength->get() * 0.9f : 0.0f));
        const float range = move_range->get();

        // Several incommensurate oscillations keep the blobs from visibly looping together.
        float x = 0.5f
                + range * 0.42f * std::sin(current_time * speed_scale * (0.72f + 0.07f * (i % 5)) + phase)
                + range * 0.11f * std::sin(current_time * speed_scale * 1.83f + phase * 0.37f);
        float y = 0.5f
                + range * 0.38f * std::cos(current_time * speed_scale * (0.61f + 0.05f * (i % 7)) + phase * 0.83f)
                + range * 0.13f * std::sin(current_time * speed_scale * 1.37f + phase * 0.51f);

        x = std::clamp(x, -0.15f, 1.15f);
        y = std::clamp(y, -0.15f, 1.15f);

        const float min_dimension = static_cast<float>(std::min(matrix_width, matrix_height));
        const float audio_radius = audio_reactive->get() ? audio_bass * audio_strength->get() * 0.42f : 0.0f;
        const float radius_wave = 0.88f + audio_radius + 0.20f * std::sin(current_time * speed_scale * 0.9f + phase);
        const float radius = min_dimension * (0.075f + 0.022f * static_cast<float>(i % 4)) * radius_wave;

        return Blob(x * static_cast<float>(matrix_width),
                    y * static_cast<float>(matrix_height),
                    std::max(2.0f, radius));
    }

    float MetaBlobScene::calculate_field(float x, float y, const Blob &blob) const {
        const float dx = x - blob.x;
        const float dy = y - blob.y;
        const float distance_squared = dx * dx + dy * dy + 1.0f;
        // Normalized metaball field. A value near one is close to a blob's nominal edge.
        return (blob.radius * blob.radius) / distance_squared;
    }

    MetaBlobScene::MetaBlobScene()
            : Scene(), time(0.0f) {
    }

    void MetaBlobScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        blobs.clear();
        blobs.reserve(std::max(1, num_blobs->get()));
        time = 0.0f;
    }

    bool MetaBlobScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();
        const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.10f);

        if (audio_reactive->get()) {
            const auto audio = AudioState::snapshot();
            const bool has_audio = audio.fresh();
            const float response = 1.0f - std::exp(-dt * 8.0f);
            audio_bass += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f) - audio_bass) * response;
            audio_mids += ((has_audio ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f : 0.0f) - audio_mids) * response;
            audio_treble += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f) - audio_treble) * response;
            if (has_audio && audio.beat_counter != last_beat_counter) { last_beat_counter = audio.beat_counter; audio_bass = std::max(audio_bass, 0.85f); }
        } else { audio_bass = audio_mids = audio_treble = 0.0f; }

        blobs.clear();
        const int blob_count = std::max(1, num_blobs->get());
        if (static_cast<int>(blobs.capacity()) < blob_count) {
            blobs.reserve(blob_count);
        }
        for (int i = 0; i < blob_count; ++i) {
            blobs.push_back(get_blob(i, time));
        }

        const float user_threshold = std::max(0.000001f, threshold->get());
        // Preserve the old threshold control while mapping its historical default to a useful field level.
        const float surface = std::clamp(user_threshold / 0.0003f, 0.15f, 4.0f) * 1.18f;
        const float base_hue = std::fmod(time * color_speed->get(), 1.0f);
        const float inv_width = 1.0f / std::max(1, matrix_width);
        const float inv_height = 1.0f / std::max(1, matrix_height);

        for (int y = 0; y < matrix_height; ++y) {
            for (int x = 0; x < matrix_width; ++x) {
                float field = 0.0f;
                float nearest = 1.0e9f;
                for (const auto &blob: blobs) {
                    field += calculate_field(static_cast<float>(x), static_cast<float>(y), blob);
                    const float dx = static_cast<float>(x) - blob.x;
                    const float dy = static_cast<float>(y) - blob.y;
                    nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy) / blob.radius);
                }

                // Soft outer aura plus a bright liquid body and a thin iridescent rim.
                const float body = smoothstep(surface * 0.76f, surface * 1.20f, field);
                const float aura = smoothstep(surface * 0.18f, surface * 0.82f, field) * (1.0f - body * 0.72f);
                const float rim_distance = std::abs(field - surface) / surface;
                const float rim_width = 0.22f + (audio_reactive->get() ? audio_treble * audio_strength->get() * 0.12f : 0.0f);
                const float rim = 1.0f - smoothstep(0.0f, rim_width, rim_distance);

                if (body <= 0.001f && rim <= 0.001f && aura <= 0.001f) {
                    continue;
                }

                const float nx = static_cast<float>(x) * inv_width;
                const float ny = static_cast<float>(y) * inv_height;
                const float hue = base_hue
                                + 0.10f * std::sin(nx * 2.0f * PI + time * 0.35f)
                                + 0.08f * std::cos(ny * 2.0f * PI - time * 0.27f)
                                + 0.035f * field;
                const auto [base_r, base_g, base_b] = hue_to_rgb(hue);
                const auto [rim_r, rim_g, rim_b] = hue_to_rgb(hue + 0.18f);

                const float inner_glow = std::clamp((1.20f - nearest) * 0.32f, 0.0f, 0.30f);
                const float brightness = std::clamp(body * 0.76f + rim * 0.62f + aura * 0.075f + inner_glow, 0.0f, 1.0f);
                const float rim_mix = std::clamp(rim * 0.82f, 0.0f, 1.0f);

                const float mixed_r = static_cast<float>(base_r) * (1.0f - rim_mix) + static_cast<float>(rim_r) * rim_mix;
                const float mixed_g = static_cast<float>(base_g) * (1.0f - rim_mix) + static_cast<float>(rim_g) * rim_mix;
                const float mixed_b = static_cast<float>(base_b) * (1.0f - rim_mix) + static_cast<float>(rim_b) * rim_mix;

                const uint8_t r = static_cast<uint8_t>(std::clamp(mixed_r * brightness, 0.0f, 255.0f));
                const uint8_t g = static_cast<uint8_t>(std::clamp(mixed_g * brightness, 0.0f, 255.0f));
                const uint8_t b = static_cast<uint8_t>(std::clamp(mixed_b * brightness, 0.0f, 255.0f));
                canvas->SetPixel(x, y, r, g, b);
            }
        }

        time += dt;
        wait_until_next_frame();
        return true;
    }

    std::string MetaBlobScene::get_name() const {
        return "metablob";
    }

    void MetaBlobScene::register_properties() {
        num_blobs->label("Blob count").description("Number of metaballs contributing to the liquid field.").group("Pattern");
        threshold->label("Surface threshold").description("Controls how strongly nearby blobs merge into one surface.").group("Pattern").step(0.00001);
        speed->label("Motion speed").description("Base drift speed of the metaballs.").group("Motion").step(0.01);
        move_range->label("Travel range").description("How far blobs wander from the center of the matrix.").group("Motion").step(0.05);
        color_speed->label("Color drift").description("Speed of the iridescent palette rotation.").group("Appearance").step(0.005);
        audio_reactive->label("Audio reactive").description("Let bass expand blobs, mids accelerate them and treble sharpen the rim.").group("Audio");
        audio_strength->label("Audio strength").description("Overall amount of music-driven modulation.").group("Audio").visible_if("audio_reactive", true).step(0.05);

        add_property(num_blobs);
        add_property(threshold);
        add_property(speed);
        add_property(move_range);
        add_property(audio_reactive);
        add_property(audio_strength);
        add_property(color_speed);
    }

    std::unique_ptr<Scenes::Scene> MetaBlobSceneWrapper::create() {
        return std::make_unique<MetaBlobScene>();
    }
}

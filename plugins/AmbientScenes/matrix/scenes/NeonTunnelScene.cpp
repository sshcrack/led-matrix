#include "NeonTunnelScene.h"

#include <shared/matrix/utils/color.h>

#include <algorithm>
#include <cmath>
#include <shared/matrix/audio_state.h>

namespace AmbientScenes {
    namespace {
        constexpr float PI = 3.14159265358979323846f;

        float smoothstep(float edge0, float edge1, float value) {
            if (edge0 == edge1) return value < edge0 ? 0.0f : 1.0f;
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float wrapped_distance(float value) {
            const float fraction = value - std::floor(value);
            return std::min(fraction, 1.0f - fraction);
        }
    }

    NeonTunnelScene::NeonTunnelScene() : Scene() {
    }

    void NeonTunnelScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        time_counter = 0.0f;
    }

    bool NeonTunnelScene::render(rgb_matrix::FrameCanvas *canvas) {
        const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.10f);
        time_counter += dt;

        if (audio_reactive->get()) {
            const auto audio = AudioState::snapshot();
            const bool has_audio = audio.fresh();
            const float response = 1.0f - std::exp(-dt * 9.0f);
            audio_bass += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f) - audio_bass) * response;
            audio_mids += ((has_audio ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f : 0.0f) - audio_mids) * response;
            audio_treble += ((has_audio ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f) - audio_treble) * response;
            if (has_audio && audio.beat_counter != last_beat_counter) { last_beat_counter = audio.beat_counter; beat_pulse = 1.0f; }
        } else { audio_bass = audio_mids = audio_treble = 0.0f; }
        beat_pulse = std::max(0.0f, beat_pulse - dt * 2.8f);

        const float center_x = static_cast<float>(matrix_width) * 0.5f;
        const float center_y = static_cast<float>(matrix_height) * 0.5f;
        const float min_dimension = static_cast<float>(std::max(1, std::min(matrix_width, matrix_height)));

        // A slowly wandering vanishing point and camera roll make the flight feel less synthetic.
        const float osc_x = center_x
                          + std::sin(time_counter * 0.31f) * static_cast<float>(matrix_width) * 0.16f
                          + std::sin(time_counter * 0.087f) * static_cast<float>(matrix_width) * 0.06f;
        const float osc_y = center_y
                          + std::cos(time_counter * 0.27f) * static_cast<float>(matrix_height) * 0.14f
                          + std::sin(time_counter * 0.113f) * static_cast<float>(matrix_height) * 0.05f;
        const float roll = std::sin(time_counter * 0.19f) * 0.42f;
        const float cos_roll = std::cos(roll);
        const float sin_roll = std::sin(roll);

        const float reactive_speed = 1.0f + (audio_reactive->get() ? audio_bass * audio_strength->get() * 1.5f : 0.0f);
        const float travel = time_counter * std::max(0.05f, speed->get()) * 0.42f * reactive_speed;
        const float angular_frequency = std::max(1.0f, angle_factor->get()) + (audio_reactive->get() ? audio_treble * audio_strength->get() * 5.0f : 0.0f);
        const float depth_scale = std::max(1.0f, distance_factor->get()) / min_dimension;

        for (int y = 0; y < matrix_height; ++y) {
            for (int x = 0; x < matrix_width; ++x) {
                const float raw_dx = static_cast<float>(x) - osc_x;
                const float raw_dy = static_cast<float>(y) - osc_y;
                const float dx = raw_dx * cos_roll - raw_dy * sin_roll;
                const float dy = raw_dx * sin_roll + raw_dy * cos_roll;

                const float distance = std::max(0.65f, std::sqrt(dx * dx + dy * dy));
                const float angle = std::atan2(dy, dx);

                // Reciprocal distance gives strong forward perspective. The two phase systems create
                // depth rings and radial ribs without the old harsh XOR checkerboard.
                const float depth_coordinate = depth_scale * min_dimension / distance + travel;
                const float ring_distance = wrapped_distance(depth_coordinate * 0.55f);
                const float ring_line = 1.0f - smoothstep(0.025f, 0.115f, ring_distance);

                const float radial_coordinate = angle / (2.0f * PI) * angular_frequency
                                              + (0.11f + audio_mids * audio_strength->get() * 0.10f) * std::sin(depth_coordinate * 1.7f - time_counter * 0.8f);
                const float rib_distance = wrapped_distance(radial_coordinate);
                const float rib_line = 1.0f - smoothstep(0.018f, 0.095f, rib_distance);

                const float crossing = ring_line * rib_line;
                const float grid = std::max(ring_line * 0.78f, rib_line * 0.58f);
                const float glow = std::max(
                    1.0f - smoothstep(0.10f, 0.30f, ring_distance),
                    (1.0f - smoothstep(0.08f, 0.25f, rib_distance)) * 0.62f
                );

                const float normalized_distance = distance / (min_dimension * 0.72f);
                const float edge_fade = 1.0f - smoothstep(0.90f, 1.75f, normalized_distance);
                const float center_fade = smoothstep(0.0f, 0.055f, distance / min_dimension);
                const float pulse = 0.82f + 0.18f * std::sin(depth_coordinate * 3.0f - time_counter * 1.7f);

                float brightness = (grid * 0.62f + glow * 0.20f + crossing * 0.42f) * edge_fade * center_fade * pulse;
                brightness *= 1.0f + audio_bass * audio_strength->get() * 0.55f + beat_pulse * 0.35f;
                brightness = std::clamp(brightness, 0.0f, 1.0f);

                // Give the vanishing point a restrained bloom rather than leaving a black singularity.
                const float core = (1.0f - smoothstep(0.0f, min_dimension * 0.055f, distance)) * 0.52f;
                brightness = std::max(brightness, core);

                const float hue = std::fmod(
                    time_counter * hue_shift_speed->get() * 38.0f
                    + depth_coordinate * 31.0f
                    + angle * 18.0f / PI,
                    360.0f
                );
                const float lightness = std::clamp(brightness * 0.53f, 0.0f, 0.56f);
                const float saturation = 0.82f + crossing * 0.18f;

                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;
                color::hsl_to_rgb(hue, saturation, lightness, r, g, b);

                // White highlights at grid crossings add depth and make motion readable from afar.
                if (crossing > 0.55f) {
                    const float highlight = std::clamp((crossing - 0.55f) * 1.35f, 0.0f, 0.48f);
                    r = static_cast<uint8_t>(std::clamp(static_cast<float>(r) + (255.0f - r) * highlight, 0.0f, 255.0f));
                    g = static_cast<uint8_t>(std::clamp(static_cast<float>(g) + (255.0f - g) * highlight, 0.0f, 255.0f));
                    b = static_cast<uint8_t>(std::clamp(static_cast<float>(b) + (255.0f - b) * highlight, 0.0f, 255.0f));
                }

                canvas->SetPixel(x, y, r, g, b);
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string NeonTunnelScene::get_name() const {
        return "neontunnel";
    }

    void NeonTunnelScene::register_properties() {
        speed->label("Flight speed").description("Base forward speed through the tunnel.").group("Motion").step(0.1);
        distance_factor->label("Depth density").description("Controls perspective spacing between tunnel rings.").group("Geometry").step(5.0);
        angle_factor->label("Radial ribs").description("Number of radial divisions around the tunnel.").group("Geometry").step(1.0);
        hue_shift_speed->label("Color drift").description("Speed of the neon palette rotation.").group("Appearance").step(0.1);
        audio_reactive->label("Audio reactive").description("Let bass drive forward motion, mids bend ribs and treble add detail.").group("Audio");
        audio_strength->label("Audio strength").description("Overall amount of music-driven modulation.").group("Audio").visible_if("audio_reactive", true).step(0.05);

        add_property(speed);
        add_property(distance_factor);
        add_property(angle_factor);
        add_property(audio_reactive);
        add_property(audio_strength);
        add_property(hue_shift_speed);
    }

    std::unique_ptr<Scenes::Scene> NeonTunnelSceneWrapper::create() {
        return std::make_unique<NeonTunnelScene>();
    }
}

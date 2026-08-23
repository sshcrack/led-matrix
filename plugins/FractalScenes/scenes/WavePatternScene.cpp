#include "WavePatternScene.h"

#include <shared/matrix/audio_state.h>
#include <shared/matrix/utils/color.h>

#include <algorithm>
#include <cmath>

using namespace Scenes;

namespace {
constexpr float TAU = 6.28318530717958647692f;
}

WavePatternScene::WavePatternScene() : Scene()
{
    set_target_fps(60);
}

void WavePatternScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    total_time_ = 0.0f;
    audio_bass_ = audio_mids_ = audio_treble_ = 0.0f;
    beat_pulse_ = drop_pulse_ = 0.0f;
    last_beat_counter_ = last_drop_counter_ = 0;
    init_waves();
}

void WavePatternScene::init_waves()
{
    waves_.clear();
    std::uniform_real_distribution<float> amplitude_dist(0.35f, 1.0f);
    std::uniform_real_distribution<float> frequency_dist(0.65f, 3.8f);
    std::uniform_real_distribution<float> speed_dist(0.45f, 1.8f);
    std::uniform_real_distribution<float> phase_dist(0.0f, TAU);
    const int count = std::clamp(num_waves_->get(), 1, 10);
    waves_.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        waves_.push_back({
            amplitude_dist(rng_),
            frequency_dist(rng_),
            speed_dist(rng_),
            phase_dist(rng_),
        });
    }
}

void WavePatternScene::update_audio(float dt)
{
    if (!audio_reactive_->get()) {
        audio_bass_ = audio_mids_ = audio_treble_ = 0.0f;
        beat_pulse_ = drop_pulse_ = 0.0f;
        return;
    }

    const auto audio = AudioState::snapshot();
    const bool fresh = audio.fresh();
    const float response = 1.0f - std::exp(-dt * 9.0f);
    const float bass = fresh ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f;
    const float mids = fresh ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) +
                                audio.feature(AudioProtocol::Feature::HighMid)) /
                                   3.0f
                             : 0.0f;
    const float treble = fresh ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f;
    audio_bass_ += (bass - audio_bass_) * response;
    audio_mids_ += (mids - audio_mids_) * response;
    audio_treble_ += (treble - audio_treble_) * response;

    if (fresh && audio.beat_counter != last_beat_counter_) {
        last_beat_counter_ = audio.beat_counter;
        beat_pulse_ = std::max(beat_pulse_, 0.6f + audio.feature(AudioProtocol::Feature::Kick) * 0.4f);
    }
    if (fresh && audio.drop_counter != last_drop_counter_) {
        last_drop_counter_ = audio.drop_counter;
        drop_pulse_ = 1.0f;
    }

    beat_pulse_ = std::max(0.0f, beat_pulse_ - dt * 3.1f);
    drop_pulse_ = std::max(0.0f, drop_pulse_ - dt * 1.2f);
}

float WavePatternScene::sample_wave(float normalized_x, float layer_phase) const
{
    if (waves_.empty())
        return 0.0f;
    float value = 0.0f;
    float amplitude_sum = 0.0f;
    for (const auto& wave : waves_) {
        value += wave.amplitude *
                 std::sin(normalized_x * wave.frequency * TAU + total_time_ * wave.phase_speed + wave.phase_offset + layer_phase);
        amplitude_sum += wave.amplitude;
    }
    return value / std::max(0.001f, amplitude_sum);
}

void WavePatternScene::add_pixel(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height())
        return;
    uint8_t old_r = 0, old_g = 0, old_b = 0;
    canvas->GetPixel(x, y, &old_r, &old_g, &old_b);
    canvas->SetPixel(x, y, static_cast<uint8_t>(std::min(255, static_cast<int>(old_r) + static_cast<int>(r))),
                     static_cast<uint8_t>(std::min(255, static_cast<int>(old_g) + static_cast<int>(g))),
                     static_cast<uint8_t>(std::min(255, static_cast<int>(old_b) + static_cast<int>(b))));
}

bool WavePatternScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.10f);
    update_audio(dt);

    const float strength = audio_reactive_->get() ? audio_strength_->get() : 0.0f;
    const float motion = 1.0f + audio_mids_ * strength * 0.75f + audio_bass_ * strength * 0.24f + drop_pulse_ * strength * 0.18f;
    total_time_ += dt * speed_->get() * motion;

    canvas->Clear();
    const int width = matrix_width;
    const int height = matrix_height;
    const int layer_count = std::clamp(ribbon_layers_->get(), 1, 9);
    const int glow = std::clamp(glow_radius_->get(), 0, 5);
    const float center_y = static_cast<float>(height - 1) * 0.5f;
    const float layer_spacing = static_cast<float>(height) / static_cast<float>(layer_count + 2) * 0.72f;
    const float amplitude =
        static_cast<float>(height) * 0.19f * wave_height_->get() * (1.0f + audio_bass_ * strength * 0.42f + drop_pulse_ * strength * 0.22f);

    for (int layer = 0; layer < layer_count; ++layer) {
        const float centered_layer = static_cast<float>(layer) - static_cast<float>(layer_count - 1) * 0.5f;
        const float layer_phase = centered_layer * 0.48f;
        const float layer_center = center_y + centered_layer * layer_spacing;
        const float layer_position = layer_count <= 1 ? 0.5f : static_cast<float>(layer) / static_cast<float>(layer_count - 1);

        for (int x = 0; x < width; ++x) {
            const float normalized_x = width <= 1 ? 0.0f : static_cast<float>(x) / static_cast<float>(width - 1);
            const float wave_value = sample_wave(normalized_x, layer_phase);
            const int y = static_cast<int>(std::lround(layer_center + wave_value * amplitude));
            const float intensity = std::clamp(
                0.55f + std::fabs(wave_value) * 0.22f + beat_pulse_ * strength * 0.22f + audio_treble_ * strength * 0.18f, 0.0f, 1.0f);

            uint8_t r = 0, g = 0, b = 0;
            if (rainbow_mode_->get()) {
                const float hue = std::fmod(normalized_x * 250.0f + layer_position * 82.0f + total_time_ * color_speed_->get() * 28.0f +
                                                audio_treble_ * strength * 70.0f,
                                            360.0f);
                color::hsv_to_rgb(hue, 0.84f, intensity, r, g, b);
            }
            else {
                const float warmth = std::clamp(layer_position + audio_bass_ * strength * 0.20f, 0.0f, 1.0f);
                r = static_cast<uint8_t>(std::clamp((55.0f + warmth * 120.0f) * intensity, 0.0f, 255.0f));
                g = static_cast<uint8_t>(std::clamp((150.0f + warmth * 80.0f) * intensity, 0.0f, 255.0f));
                b = static_cast<uint8_t>(std::clamp((255.0f - warmth * 35.0f) * intensity, 0.0f, 255.0f));
            }

            add_pixel(canvas, x, y, r, g, b);
            for (int distance = 1; distance <= glow; ++distance) {
                const float falloff = 0.34f / static_cast<float>(distance);
                add_pixel(canvas, x, y - distance, static_cast<uint8_t>(r * falloff), static_cast<uint8_t>(g * falloff),
                          static_cast<uint8_t>(b * falloff));
                add_pixel(canvas, x, y + distance, static_cast<uint8_t>(r * falloff), static_cast<uint8_t>(g * falloff),
                          static_cast<uint8_t>(b * falloff));
            }
        }
    }

    wait_until_next_frame();
    return true;
}

string WavePatternScene::get_name() const
{
    return "wave_pattern";
}

void WavePatternScene::register_properties()
{
    num_waves_->label("Wave voices").description("Number of frequencies combined into each ribbon.").group("Pattern");
    speed_->label("Motion speed").description("Base phase speed of the wave field.").group("Motion").step(0.05);
    wave_height_->label("Wave height").description("Vertical amplitude of each ribbon.").group("Pattern").step(0.05);
    ribbon_layers_->label("Ribbon layers").description("Number of luminous wave ribbons drawn across the matrix.").group("Pattern");
    glow_radius_->label("Glow radius").description("Soft vertical halo around each ribbon.").group("Appearance").unit("px");
    rainbow_mode_->label("Rainbow palette")
        .description("Use a drifting multi-color palette instead of the cool cyan theme.")
        .group("Appearance");
    color_speed_->label("Color drift")
        .description("Palette animation speed.")
        .group("Appearance")
        .visible_if("rainbow_mode", true)
        .step(0.05);
    audio_reactive_->label("Audio reactive")
        .description("Let bass shape amplitude, mids drive motion and treble drive color and glow.")
        .group("Audio");
    audio_strength_->label("Audio strength")
        .description("Overall amount of music-driven modulation.")
        .group("Audio")
        .visible_if("audio_reactive", true)
        .step(0.05);

    add_property(num_waves_);
    add_property(speed_);
    add_property(color_speed_);
    add_property(rainbow_mode_);
    add_property(wave_height_);
    add_property(ribbon_layers_);
    add_property(glow_radius_);
    add_property(audio_reactive_);
    add_property(audio_strength_);
}

void WavePatternScene::load_properties(const json& j)
{
    Scene::load_properties(j);
    init_waves();
}

std::unique_ptr<Scene> WavePatternSceneWrapper::create()
{
    return std::make_unique<WavePatternScene>();
}

Scenes::SceneDescriptor Scenes::WavePatternScene::get_descriptor() const {
    auto d = Scene::get_descriptor(); d.automatic_eligible = true;
    d.family = "waves";
    d.tags = {"ambient", "waves", "ribbons", "flow", "audio-reactive"};
    d.intensity = 0.44f; d.motion = 0.52f; d.music_affinity = 0.64f; d.performance_cost = 0.56f;
    d.variants = {
        {"silk", "Silk ribbons", "Fewer slow layers for a soft ambient look",
         {{"num_waves", 3}, {"speed", 0.42f}, {"color_speed", 0.28f}, {"wave_height", 0.65f}, {"ribbon_layers", 4}, {"glow_radius", 1}, {"audio_reactive", false}},
         {"calm", "soft"}, 0.22f, 0.30f, 0.16f, 0.44f},
        {"layered", "Layered waves", "Dense bright ribbons with faster movement",
         {{"num_waves", 6}, {"speed", 1.2f}, {"color_speed", 0.85f}, {"wave_height", 1.2f}, {"ribbon_layers", 7}, {"glow_radius", 3}},
         {"dense", "vivid"}, 0.70f, 0.72f, 0.45f, 0.72f},
        {"music", "Music ribbons", "Flowing ribbons shaped by the current track",
         {{"num_waves", 4}, {"speed", 0.85f}, {"audio_reactive", true}, {"audio_strength", 1.1f}},
         {"music", "beat-driven"}, 0.64f, 0.68f, 1.0f, 0.62f},
    };
    return d;
}

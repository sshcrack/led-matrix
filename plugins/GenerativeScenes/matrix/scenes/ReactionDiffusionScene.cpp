#include "ReactionDiffusionScene.h"

#include <shared/matrix/audio_state.h>

#include <algorithm>
#include <cmath>

namespace GenerativeScenes {

ReactionDiffusionScene::ReactionDiffusionScene()
{
    set_target_fps(30);
}

void ReactionDiffusionScene::register_properties()
{
    simulation_speed_->label("Simulation speed").description("How quickly the chemical pattern evolves.").group("Motion").step(0.05);
    color_speed_->label("Color drift").description("Speed of the slow palette rotation.").group("Appearance").step(0.05);
    contrast_->label("Pattern contrast")
        .description("Separates the dark background from active chemical regions.")
        .group("Appearance")
        .step(0.05);
    audio_reactive_->label("Audio reactive").description("Let music reshape, accelerate and recolor the reaction pattern.").group("Audio");
    audio_strength_->label("Audio strength")
        .description("Overall amount of music-driven modulation and beat seeding.")
        .group("Audio")
        .visible_if("audio_reactive", true)
        .step(0.05);

    add_property(simulation_speed_);
    add_property(color_speed_);
    add_property(contrast_);
    add_property(audio_reactive_);
    add_property(audio_strength_);
}

void ReactionDiffusionScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    const size_t size = static_cast<size_t>(std::max(0, matrix_width) * std::max(0, matrix_height));
    u_cur_.assign(size, 1.0f);
    v_cur_.assign(size, 0.0f);
    u_next_.assign(size, 1.0f);
    v_next_.assign(size, 0.0f);
    current_preset_ = 0;
    step_count_ = 0;
    global_hue_ = 0.0f;
    audio_bass_ = audio_mids_ = audio_treble_ = audio_balance_ = 0.0f;
    beat_pulse_ = drop_pulse_ = 0.0f;
    last_beat_counter_ = last_drop_counter_ = 0;
    simulation_.reset();
    seed_random_patch();
}

void ReactionDiffusionScene::seed_patch(int center_x, int center_y, int radius, float concentration)
{
    if (matrix_width <= 0 || matrix_height <= 0)
        return;
    const int r = std::max(1, radius);
    const float r2 = static_cast<float>(r * r);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const float distance2 = static_cast<float>(dx * dx + dy * dy);
            if (distance2 > r2)
                continue;
            const int x = (center_x + dx + matrix_width) % matrix_width;
            const int y = (center_y + dy + matrix_height) % matrix_height;
            const float falloff = 1.0f - distance2 / std::max(1.0f, r2);
            const int idx = index(x, y);
            v_cur_[idx] = std::max(v_cur_[idx], concentration * (0.55f + 0.45f * falloff));
            u_cur_[idx] = std::min(u_cur_[idx], 0.22f * (1.0f - falloff));
        }
    }
}

void ReactionDiffusionScene::seed_random_patch()
{
    if (matrix_width <= 2 || matrix_height <= 2)
        return;
    std::uniform_int_distribution<int> x_dist(0, matrix_width - 1);
    std::uniform_int_distribution<int> y_dist(0, matrix_height - 1);
    const int min_dimension = std::min(matrix_width, matrix_height);
    const int radius = std::clamp(min_dimension / 32, 2, 5);
    const int seed_count = 6 + static_cast<int>(rng_() % 8);
    for (int i = 0; i < seed_count; ++i) seed_patch(x_dist(rng_), y_dist(rng_), radius);
}

void ReactionDiffusionScene::reset_pattern(bool advance_preset)
{
    if (advance_preset)
        current_preset_ = (current_preset_ + 1) % NUM_PRESETS;
    std::fill(u_cur_.begin(), u_cur_.end(), 1.0f);
    std::fill(v_cur_.begin(), v_cur_.end(), 0.0f);
    std::fill(u_next_.begin(), u_next_.end(), 1.0f);
    std::fill(v_next_.begin(), v_next_.end(), 0.0f);
    seed_random_patch();
    step_count_ = 0;
}

void ReactionDiffusionScene::simulation_step(float feed, float kill)
{
    const int width = matrix_width;
    const int height = matrix_height;
    for (int y = 0; y < height; ++y) {
        const int y_prev = (y - 1 + height) % height;
        const int y_next = (y + 1) % height;
        for (int x = 0; x < width; ++x) {
            const int x_prev = (x - 1 + width) % width;
            const int x_next = (x + 1) % width;
            const int idx = index(x, y);
            const float u = u_cur_[idx];
            const float v = v_cur_[idx];
            const float lap_u =
                u_cur_[index(x_next, y)] + u_cur_[index(x_prev, y)] + u_cur_[index(x, y_next)] + u_cur_[index(x, y_prev)] - 4.0f * u;
            const float lap_v =
                v_cur_[index(x_next, y)] + v_cur_[index(x_prev, y)] + v_cur_[index(x, y_next)] + v_cur_[index(x, y_prev)] - 4.0f * v;
            const float reaction = u * v * v;
            u_next_[idx] = std::clamp(u + DT * (DU * lap_u - reaction + feed * (1.0f - u)), 0.0f, 1.0f);
            v_next_[idx] = std::clamp(v + DT * (DV * lap_v + reaction - (feed + kill) * v), 0.0f, 1.0f);
        }
    }
    std::swap(u_cur_, u_next_);
    std::swap(v_cur_, v_next_);
}

void ReactionDiffusionScene::inject_audio_event(bool drop)
{
    if (matrix_width <= 0 || matrix_height <= 0)
        return;
    std::uniform_int_distribution<int> x_dist(0, matrix_width - 1);
    std::uniform_int_distribution<int> y_dist(0, matrix_height - 1);
    const int min_dimension = std::min(matrix_width, matrix_height);
    const int radius = std::clamp(min_dimension / (drop ? 13 : 25), 2, drop ? 12 : 7);
    const int count = drop ? 4 : 1;
    for (int i = 0; i < count; ++i) {
        int x = x_dist(rng_);
        const int stereo_offset = static_cast<int>(audio_balance_ * matrix_width * 0.18f);
        x = (x + stereo_offset + matrix_width) % matrix_width;
        seed_patch(x, y_dist(rng_), radius, drop ? 1.0f : 0.82f);
    }
}

void ReactionDiffusionScene::update_audio(float dt)
{
    if (!audio_reactive_->get()) {
        audio_bass_ = audio_mids_ = audio_treble_ = audio_balance_ = 0.0f;
        beat_pulse_ = drop_pulse_ = 0.0f;
        return;
    }

    const auto audio = AudioState::snapshot();
    const bool fresh = audio.fresh();
    const float response = 1.0f - std::exp(-dt * 8.5f);
    const float target_bass =
        fresh ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass)) : 0.0f;
    const float target_mids = fresh ? (audio.feature(AudioProtocol::Feature::LowMid) + audio.feature(AudioProtocol::Feature::Mid) +
                                       audio.feature(AudioProtocol::Feature::HighMid)) /
                                          3.0f
                                    : 0.0f;
    const float target_treble =
        fresh ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air)) : 0.0f;
    const float target_balance = fresh ? audio.feature(AudioProtocol::Feature::StereoBalance) : 0.0f;
    audio_bass_ += (target_bass - audio_bass_) * response;
    audio_mids_ += (target_mids - audio_mids_) * response;
    audio_treble_ += (target_treble - audio_treble_) * response;
    audio_balance_ += (target_balance - audio_balance_) * response;

    if (fresh && audio.beat_counter != last_beat_counter_) {
        last_beat_counter_ = audio.beat_counter;
        beat_pulse_ = std::max(beat_pulse_, 0.55f + audio.feature(AudioProtocol::Feature::Kick) * 0.45f);
        if (audio.feature(AudioProtocol::Feature::Kick) > 0.28f)
            inject_audio_event(false);
    }
    if (fresh && audio.drop_counter != last_drop_counter_) {
        last_drop_counter_ = audio.drop_counter;
        drop_pulse_ = 1.0f;
        inject_audio_event(true);
    }

    beat_pulse_ = std::max(0.0f, beat_pulse_ - dt * 2.2f);
    drop_pulse_ = std::max(0.0f, drop_pulse_ - dt * 0.9f);
}

std::tuple<uint8_t, uint8_t, uint8_t> ReactionDiffusionScene::palette(float value, float hue_shift, float contrast)
{
    if (value < 0.025f)
        return {0, 0, 8};

    float t = std::clamp(value * 2.65f, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    t = std::pow(t, 1.0f / std::clamp(contrast, 0.5f, 2.0f));
    float hue = std::fmod(hue_shift + 0.58f - t * 0.48f, 1.0f);
    if (hue < 0.0f)
        hue += 1.0f;

    const float saturation = 0.92f;
    const float brightness = 0.10f + t * 0.90f;
    const float h6 = hue * 6.0f;
    const int sector = static_cast<int>(h6);
    const float f = h6 - static_cast<float>(sector);
    const float p = brightness * (1.0f - saturation);
    const float q = brightness * (1.0f - saturation * f);
    const float tv = brightness * (1.0f - saturation * (1.0f - f));

    float r = 0.0f, g = 0.0f, b = 0.0f;
    switch (sector % 6) {
        case 0:
            r = brightness;
            g = tv;
            b = p;
            break;
        case 1:
            r = q;
            g = brightness;
            b = p;
            break;
        case 2:
            r = p;
            g = brightness;
            b = tv;
            break;
        case 3:
            r = p;
            g = q;
            b = brightness;
            break;
        case 4:
            r = tv;
            g = p;
            b = brightness;
            break;
        default:
            r = brightness;
            g = p;
            b = q;
            break;
    }

    return {
        static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f)),
    };
}

bool ReactionDiffusionScene::render(rgb_matrix::FrameCanvas* canvas)
{
    if (u_cur_.size() != static_cast<size_t>(matrix_width * matrix_height))
        initialize(matrix_width, matrix_height);

    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.20f);
    update_audio(dt);

    const float strength = audio_reactive_->get() ? audio_strength_->get() : 0.0f;
    const float speed_scale = simulation_speed_->get() * (1.0f + audio_bass_ * strength * 0.42f + drop_pulse_ * strength * 0.22f);
    simulation_.configure(30.0 * speed_scale, 4);

    const Preset& preset = PRESETS[current_preset_];
    const float feed = std::clamp(preset.feed + (audio_mids_ - 0.35f) * strength * 0.0028f, 0.015f, 0.075f);
    const float kill = std::clamp(preset.kill + audio_treble_ * strength * 0.0014f - audio_bass_ * strength * 0.0010f, 0.045f, 0.072f);

    simulation_.advance(dt, [&](double) {
        for (int step = 0; step < SIM_STEPS_PER_TICK; ++step) simulation_step(feed, kill);
        step_count_ += SIM_STEPS_PER_TICK;
    });

    if (step_count_ >= STEPS_PER_PRESET)
        reset_pattern(true);

    global_hue_ = std::fmod(global_hue_ + dt * (0.0075f * color_speed_->get() + audio_treble_ * strength * 0.012f), 1.0f);

    const float visual_boost = 1.0f + beat_pulse_ * strength * 0.10f + drop_pulse_ * strength * 0.16f;
    for (int y = 0; y < matrix_height; ++y) {
        for (int x = 0; x < matrix_width; ++x) {
            const float value = std::clamp(v_cur_[index(x, y)] * visual_boost, 0.0f, 1.0f);
            auto [r, g, b] = palette(value, global_hue_, contrast_->get());
            canvas->SetPixel(x, y, r, g, b);
        }
    }

    wait_until_next_frame();
    return true;
}

}  // namespace GenerativeScenes

#include "MetaBlobScene.h"

#include <algorithm>
#include <cmath>
#include <shared/matrix/audio_state.h>

namespace AmbientScenes {
MetaBlobScene::MetaBlobScene() : Scene() {}

void MetaBlobScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    time = 0.0f;
    audio_bass = audio_mids = audio_treble = audio_balance = 0.0f;
    beat_pulse = drop_pulse = section_hue = 0.0f;
    last_beat_counter = last_drop_counter = last_section_counter = 0;

    // Metaballs are deliberately one of the stress-test scenes on a Pi 4.
    // Start from the visually near-identical 2x scalar-field sampling path;
    // adaptive quality can climb back to full resolution when there is room.
    set_render_quality_hint(0.84f);
}

bool MetaBlobScene::render(rgb_matrix::FrameCanvas *canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.10f);

    if (audio_reactive->get()) {
        const auto audio = AudioState::snapshot();
        const bool has_audio = audio.fresh();
        const float response = 1.0f - std::exp(-dt * 8.0f);
        audio_bass += ((has_audio
            ? 0.5f * (audio.feature(AudioProtocol::Feature::SubBass) + audio.feature(AudioProtocol::Feature::Bass))
            : 0.0f) - audio_bass) * response;
        audio_mids += ((has_audio
            ? (audio.feature(AudioProtocol::Feature::LowMid)
                + audio.feature(AudioProtocol::Feature::Mid)
                + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f
            : 0.0f) - audio_mids) * response;
        audio_treble += ((has_audio
            ? 0.5f * (audio.feature(AudioProtocol::Feature::Treble) + audio.feature(AudioProtocol::Feature::Air))
            : 0.0f) - audio_treble) * response;
        audio_balance += ((has_audio ? audio.feature(AudioProtocol::Feature::StereoBalance) : 0.0f)
            - audio_balance) * response;

        if (has_audio && audio.beat_counter != last_beat_counter) {
            last_beat_counter = audio.beat_counter;
            beat_pulse = std::max(
                beat_pulse,
                0.55f + audio.feature(AudioProtocol::Feature::Kick) * 0.45f);
        }
        if (has_audio && audio.drop_counter != last_drop_counter) {
            last_drop_counter = audio.drop_counter;
            drop_pulse = 1.0f;
        }
        if (has_audio && audio.section_counter != last_section_counter) {
            last_section_counter = audio.section_counter;
            section_hue += 0.17f;
        }
    } else {
        audio_bass = audio_mids = audio_treble = audio_balance = 0.0f;
    }

    beat_pulse = std::max(0.0f, beat_pulse - dt * 3.0f);
    drop_pulse = std::max(0.0f, drop_pulse - dt * 0.95f);

    AmbientScenes::MetaballParams params;
    params.width = matrix_width;
    params.height = matrix_height;
    params.blob_count = std::max(1, num_blobs->get());
    params.speed = speed->get();
    params.move_range = move_range->get();
    params.color_speed = color_speed->get();
    params.threshold = threshold->get();
    params.audio_reactive = audio_reactive->get();
    params.audio_strength = audio_strength->get();

    AmbientScenes::MetaballAudio audio;
    audio.bass = audio_bass;
    audio.mids = audio_mids;
    audio.treble = audio_treble;
    audio.balance = audio_balance;
    audio.beat_pulse = beat_pulse;
    audio.drop_pulse = drop_pulse;
    audio.section_hue = section_hue;

    const auto &frame = renderer_.render(params, audio, time, render_quality_scale());
    const int pixels = std::min<int>(
        matrix_width * matrix_height,
        static_cast<int>(frame.size() / 3));
    for (int index = 0; index < pixels; ++index) {
        const int x = index % matrix_width;
        const int y = index / matrix_width;
        const auto offset = static_cast<std::size_t>(index * 3);
        canvas->SetPixel(x, y, frame[offset], frame[offset + 1], frame[offset + 2]);
    }

    time += dt;
    wait_until_next_frame();
    return true;
}

std::string MetaBlobScene::get_name() const
{
    return "metablob";
}

void MetaBlobScene::register_properties()
{
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

std::unique_ptr<Scenes::Scene> MetaBlobSceneWrapper::create()
{
    return std::make_unique<MetaBlobScene>();
}
} // namespace AmbientScenes

Scenes::SceneDescriptor AmbientScenes::MetaBlobScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "organic";
    d.tags = {"ambient", "organic", "fluid", "abstract", "audio-reactive"};
    d.intensity = 0.34f;
    d.motion = 0.38f;
    d.music_affinity = 0.58f;
    d.performance_cost = 0.55f;
    d.variants = {
        {"calm", "Slow liquid", "Large relaxed blobs with gentle color motion",
         {{"num_blobs", 7}, {"speed", 0.12f}, {"move_range", 0.34f}, {"color_speed", 0.015f}, {"audio_reactive", false}},
         {"calm", "organic"}, 0.20f, 0.25f, 0.15f, 0.48f},
        {"dense", "Liquid field", "More blobs, faster mixing and richer color movement",
         {{"num_blobs", 15}, {"speed", 0.42f}, {"move_range", 0.72f}, {"color_speed", 0.060f}},
         {"dense", "fluid"}, 0.58f, 0.58f, 0.40f, 0.64f},
        {"music", "Music liquid", "Organic shapes breathe and push with the track",
         {{"num_blobs", 11}, {"speed", 0.28f}, {"audio_reactive", true}, {"audio_strength", 1.15f}},
         {"music", "beat-driven"}, 0.62f, 0.60f, 1.0f, 0.60f},
    };
    return d;
}

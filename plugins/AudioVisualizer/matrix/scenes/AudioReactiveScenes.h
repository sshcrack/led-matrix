#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "../AudioVisualizer.h"
#include <random>

namespace Scenes {

class AudioParticleFieldScene final : public Scene {
    struct Particle {
        float x{};
        float y{};
        float vx{};
        float vy{};
        float life{};
        float max_life{};
        float hue{};
        float size{};
    };

    AudioVisualizer* plugin{};
    FrameTimer timer;
    std::vector<Particle> particles;
    std::mt19937 rng{std::random_device{}()};
    uint64_t seen_beat_counter{};
    float spawn_accumulator{};
    float hue_time{};

    PropertyPointer<float> sensitivity = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.25f, 0.25f, 4.0f);
    PropertyPointer<int> particle_limit = MAKE_PROPERTY_MINMAX("particle_limit", int, 420, 40, 1500);
    PropertyPointer<float> trail_strength = MAKE_PROPERTY_MINMAX("trail_strength", float, 0.78f, 0.0f, 0.96f);
    PropertyPointer<float> gravity = MAKE_PROPERTY_MINMAX("gravity", float, 14.0f, -30.0f, 60.0f);
    PropertyPointer<bool> rainbow = MAKE_PROPERTY("rainbow", bool, true);
    PropertyPointer<rgb_matrix::Color> base_color = MAKE_PROPERTY("base_color", rgb_matrix::Color, rgb_matrix::Color(40, 180, 255));
    PropertyPointer<bool> beat_bursts = MAKE_PROPERTY("beat_bursts", bool, true);

    void spawn_particle(float bass, float mids, float treble, bool burst);
    void find_plugin();

public:
    AudioParticleFieldScene();
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    string get_name() const override { return "audio_particles"; }
    std::string get_category() const override { return "Audio Reactive"; }
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
    int get_default_weight() override { return 4; }
    [[nodiscard]] bool needs_desktop_app() override { return true; }
};

class AudioParticleFieldSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

class AudioPulseTunnelScene final : public Scene {
    AudioVisualizer* plugin{};
    FrameTimer timer;
    float travel{};
    float rotation{};
    float beat_pulse{};
    uint64_t seen_beat_counter{};

    PropertyPointer<float> sensitivity = MAKE_PROPERTY_MINMAX("sensitivity", float, 1.2f, 0.25f, 4.0f);
    PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 0.34f, 0.05f, 1.5f);
    PropertyPointer<int> ring_count = MAKE_PROPERTY_MINMAX("ring_count", int, 14, 5, 30);
    PropertyPointer<float> twist = MAKE_PROPERTY_MINMAX("twist", float, 0.7f, -3.0f, 3.0f);
    PropertyPointer<bool> rainbow = MAKE_PROPERTY("rainbow", bool, true);
    PropertyPointer<rgb_matrix::Color> base_color = MAKE_PROPERTY("base_color", rgb_matrix::Color, rgb_matrix::Color(80, 80, 255));
    PropertyPointer<bool> show_spectrum_ribs = MAKE_PROPERTY("show_spectrum_ribs", bool, true);

    void find_plugin();

public:
    AudioPulseTunnelScene();
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    string get_name() const override { return "audio_pulse_tunnel"; }
    std::string get_category() const override { return "Audio Reactive"; }
    void register_properties() override;
    tmillis_t get_default_duration() override { return 25000; }
    int get_default_weight() override { return 4; }
    [[nodiscard]] bool needs_desktop_app() override { return true; }
};

class AudioPulseTunnelSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

}

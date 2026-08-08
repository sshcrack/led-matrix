#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <random>
#include <vector>

namespace GenerativeScenes {
class BoidsScene final : public Scenes::Scene {
    struct Boid { float x, y, vx, vy; };
    std::vector<Boid> boids_;
    std::mt19937 rng_{std::random_device{}()};
    Scenes::FixedStepAccumulator simulation_{30.0, 3};
    float audio_bass_ = 0.0f, audio_mids_ = 0.0f, audio_treble_ = 0.0f;
    float audio_balance_ = 0.0f;
    float beat_pulse_ = 0.0f;
    float drop_pulse_ = 0.0f;
    uint64_t last_beat_counter_ = 0;
    uint64_t last_drop_counter_ = 0;

    PropertyPointer<int> count_ = MAKE_PROPERTY_MINMAX("count", int, 48, 8, 180);
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 0.75f, 0.12f, 2.0f);
    PropertyPointer<float> perception_ = MAKE_PROPERTY_MINMAX("perception", float, 18.0f, 4.0f, 48.0f);
    PropertyPointer<float> trail_fade_ = MAKE_PROPERTY_MINMAX("trail_fade", float, 0.78f, 0.0f, 0.96f);
    PropertyPointer<bool> audio_reactive_ = MAKE_PROPERTY("audio_reactive", bool, false);
    PropertyPointer<float> audio_strength_ = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.65f, 0.0f, 2.0f);
    PropertyPointer<bool> rainbow_ = MAKE_PROPERTY("rainbow", bool, true);
    PropertyPointer<rgb_matrix::Color> color_ = MAKE_PROPERTY("color", rgb_matrix::Color, rgb_matrix::Color(80, 210, 255));

    std::vector<uint8_t> framebuffer_;
    void reset_boids();
    void ensure_buffers();
    void simulate_step();

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    [[nodiscard]] std::string get_name() const override { return "boids"; }
    [[nodiscard]] std::string get_category() const override { return "Generative"; }
    Scenes::SceneCapabilities get_capabilities() const override { auto caps = Scenes::Scene::get_capabilities(); caps.supports_audio = true; return caps; }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 6; }
};

class BoidsSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};
}

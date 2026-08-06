#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <random>
#include <vector>

namespace GenerativeScenes {
class BoidsScene final : public Scenes::Scene {
    struct Boid { float x, y, vx, vy; };
    std::vector<Boid> boids_;
    std::mt19937 rng_{std::random_device{}()};
    std::chrono::steady_clock::time_point last_update_{};
    float simulation_accumulator_ = 0.0f;

    PropertyPointer<int> count_ = MAKE_PROPERTY("count", int, 48);
    PropertyPointer<float> speed_ = MAKE_PROPERTY("speed", float, 0.75f);
    PropertyPointer<float> perception_ = MAKE_PROPERTY("perception", float, 18.0f);
    PropertyPointer<float> trail_fade_ = MAKE_PROPERTY("trail_fade", float, 0.78f);
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
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 6; }
};

class BoidsSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};
}

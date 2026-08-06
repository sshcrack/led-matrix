#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

namespace GenerativeScenes {
class FallingSandScene final : public Scenes::Scene {
    struct Cell {
        uint8_t type = 0; // 0 empty, 1 sand, 2 water
        uint8_t hue = 0;
    };
    struct Emitter {
        float x = 0.0f;
        float velocity = 0.0f;
        float phase = 0.0f;
        uint8_t mode = 0;
        uint8_t hue_offset = 0;
        uint32_t mode_until = 0;
    };
    struct Debris {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        Cell cell{};
        uint16_t life = 0;
    };
    struct Dust {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        uint16_t life = 0;
        uint16_t max_life = 1;
        uint8_t warmth = 0;
    };

    std::vector<Cell> cells_;
    std::vector<Emitter> emitter_state_;
    std::vector<Debris> debris_;
    std::vector<Dust> dust_;
    std::mt19937 rng_{std::random_device{}()};
    uint32_t frame_ = 0;
    std::chrono::steady_clock::time_point last_update_{};
    float simulation_accumulator_ = 0.0f;
    bool draining_ = false;
    uint32_t drain_frames_ = 0;
    float wind_ = 0.0f;
    float wind_target_ = 0.0f;
    uint32_t next_wind_change_ = 0;
    uint32_t wind_end_frame_ = 0;
    uint32_t next_explosion_ = 0;
    uint32_t effect_cooldown_until_ = 0;
    bool wind_active_ = false;
    uint32_t flash_until_ = 0;
    uint32_t flash_started_ = 0;
    float flash_x_ = 0.0f, flash_y_ = 0.0f, flash_radius_ = 0.0f;

    PropertyPointer<int> emitters_ = MAKE_PROPERTY("emitters", int, 3);
    PropertyPointer<int> spawn_rate_ = MAKE_PROPERTY("spawn_rate", int, 1);
    PropertyPointer<bool> water_ = MAKE_PROPERTY("water", bool, true);
    PropertyPointer<int> reset_fill_percent_ = MAKE_PROPERTY("reset_fill_percent", int, 72);
    PropertyPointer<bool> wind_enabled_ = MAKE_PROPERTY("wind", bool, true);
    PropertyPointer<int> wind_strength_ = MAKE_PROPERTY("wind_strength", int, 55);
    PropertyPointer<bool> explosions_enabled_ = MAKE_PROPERTY("explosions", bool, true);
    PropertyPointer<int> explosion_frequency_ = MAKE_PROPERTY("explosion_frequency", int, 10);

    int index(int x, int y) const { return y * matrix_width + x; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < matrix_width && y < matrix_height; }
    void reset();
    void update_emitters();
    void update_wind();
    void trigger_explosion();
    void update_debris();
    void update_dust();
    void simulate_step();
    void draw(rgb_matrix::FrameCanvas *canvas);

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void register_properties() override;
    [[nodiscard]] std::string get_name() const override { return "falling_sand"; }
    [[nodiscard]] std::string get_category() const override { return "Generative"; }
    tmillis_t get_default_duration() override { return 90000; }
    int get_default_weight() override { return 6; }
};

class FallingSandSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};
}

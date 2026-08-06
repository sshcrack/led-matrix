#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <cstdint>
#include <chrono>

namespace AmbientScenes {
    class DigitalRainScene : public Scenes::Scene {
    private:
        struct Drop {
            int x = 0;
            float y = 0.0f;
            float speed = 1.0f;
            int length = 8;
            float depth = 1.0f;
            uint32_t seed = 0;
        };

        std::vector<Drop> drops;
        std::vector<std::vector<float>> matrix_brightness;
        std::vector<std::vector<uint8_t>> matrix_symbols;

        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<> dis_speed;
        std::uniform_int_distribution<> dis_length;
        std::uniform_int_distribution<> dis_x;
        uint64_t simulation_tick = 0;
        std::chrono::steady_clock::time_point last_update;
        float simulation_accumulator = 0.0f;
        static constexpr float simulation_step = 1.0f / 30.0f;

        PropertyPointer<int> num_drops = MAKE_PROPERTY("num_drops", int, 40);
        PropertyPointer<float> base_speed = MAKE_PROPERTY("base_speed", float, 1.0f);
        PropertyPointer<float> fade_factor = MAKE_PROPERTY("fade_factor", float, 0.90f);
        PropertyPointer<rgb_matrix::Color> color = MAKE_PROPERTY("color", rgb_matrix::Color, rgb_matrix::Color(0, 255, 70));
        PropertyPointer<bool> symbol_mode = MAKE_PROPERTY("symbol_mode", bool, true);
        PropertyPointer<bool> glitch_effect = MAKE_PROPERTY("glitch_effect", bool, true);

        void reset_drop(Drop& drop);
        void draw_symbol(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t symbol,
                         uint8_t r, uint8_t g, uint8_t b) const;

    public:
        explicit DigitalRainScene();
        ~DigitalRainScene() override = default;

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class DigitalRainSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene> create() override;
    };
}

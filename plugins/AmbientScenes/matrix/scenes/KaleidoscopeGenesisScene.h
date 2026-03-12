#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <cmath>
#include <random>

namespace AmbientScenes {
    class KaleidoscopeGenesisScene : public Scenes::Scene {
    private:
        struct Point {
            float x, y;
            uint8_t r, g, b;

            Point(float _x, float _y, uint8_t _r, uint8_t _g, uint8_t _b)
                : x(_x), y(_y), r(_r), g(_g), b(_b) {}
        };

        std::vector<Point> points;
        std::mt19937 rng;
        float hue_offset = 0;
        float pattern_phase = 0;

        PropertyPointer<int> symmetry = MAKE_PROPERTY("symmetry", int, 6);
        PropertyPointer<float> rotation_speed = MAKE_PROPERTY("rotation_speed", float, 1.0f);
        PropertyPointer<float> evolution_speed = MAKE_PROPERTY("evolution_speed", float, 0.5f);

    public:
        explicit KaleidoscopeGenesisScene();
        ~KaleidoscopeGenesisScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 50000; }
        int get_default_weight() override { return 2; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;

    private:
        void hsl_to_rgb(float h, float s, float l, uint8_t &r, uint8_t &g, uint8_t &b);
        void generate_pattern(int center_x, int center_y);
    };

    class KaleidoscopeGenesisSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

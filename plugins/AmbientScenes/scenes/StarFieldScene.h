#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class StarFieldScene : public Scenes::Scene {
    private:
        struct Star {
            float x, y, z;
            uint8_t brightness;

            Star() : x(0), y(0), z(0), brightness(255) {
            }

            void respawn(float max_depth);

            void update(float speed);
        };

        std::vector<Star> stars;
        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<> dis;

        PropertyPointer<int> num_stars = MAKE_PROPERTY("num_stars", int, 50);
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 0.02f);
        PropertyPointer<bool> enable_twinkle = MAKE_PROPERTY("enable_twinkle", bool, true);
        PropertyPointer<float> max_depth = MAKE_PROPERTY("max_depth", float, 3.0f);

    public:
        explicit StarFieldScene();

        ~StarFieldScene() override = default;

        void register_properties() override;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }

        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class StarFieldSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

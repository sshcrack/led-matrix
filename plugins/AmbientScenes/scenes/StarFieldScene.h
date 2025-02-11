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

            Star() : x(0), y(0), z(0), brightness(255) {}

            void respawn(float max_depth);

            void update(float speed);
        };

        std::vector<Star> stars;
        std::random_device rd;
        std::mt19937 gen;
        std::uniform_real_distribution<> dis;

        Property<int> num_stars = Property<int>("num_stars", 50);
        Property<float> speed = Property<float>("speed", 0.02f);
        Property<bool> enable_twinkle = Property<bool>("enable_twinkle", true);
        Property<float> max_depth = Property<float>("max_depth", 3.0f);

    public:
        explicit StarFieldScene();
        ~StarFieldScene() override = default;
        void register_properties() override;

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class StarFieldSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

#pragma once

#include "Scene.h"
#include "plugin.h"
#include <vector>
#include <random>

namespace Scenes {
    class StarFieldScene : public Scene {
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

        int num_stars;
        float speed;
        bool enable_twinkle;
        float max_depth;

    public:
        explicit StarFieldScene(const nlohmann::json &config);

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class StarFieldSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}

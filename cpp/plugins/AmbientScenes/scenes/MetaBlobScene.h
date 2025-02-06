#pragma once

#include "Scene.h"
#include "plugin.h"
#include <vector>
#include <random>

namespace Scenes {
    class MetaBlobScene : public Scene {
    private:
        struct Blob {
            float x, y;
            float radius;

            Blob(float x, float y, float radius) : x(x), y(y), radius(radius) {}
        };

        float time;
        int num_blobs;
        float threshold;
        float speed;
        float move_range;
        float color_speed;  // Add color cycling speed parameter

        std::vector<Blob> blobs;

        // Shader conversion helpers
        float rand_sin(int i) const;

        Blob get_blob(rgb_matrix::RGBMatrix *matrix, int i, float time) const;

        float calculate_field(float x, float y, const Blob &blob) const;

    public:
        explicit MetaBlobScene(const nlohmann::json &config);

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}

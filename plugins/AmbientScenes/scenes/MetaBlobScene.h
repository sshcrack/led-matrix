#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
class MetaBlobScene : public Scenes::Scene {
    private:
        Property<int> num_blobs = Property("num_blobs", 10);
        Property<float> threshold = Property("threshold", 0.0003f);
        Property<float> speed = Property("speed", 0.25f);
        Property<float> move_range = Property("move_range", 0.5f);
        Property<float> color_speed = Property("color_speed", 0.033f);

        float time;
        struct Blob {
            float x, y;
            float radius;

            Blob(float x, float y, float radius) : x(x), y(y), radius(radius) {}
        };

        std::vector<Blob> blobs;

        // Shader conversion helpers
        float rand_sin(int i) const;

        Blob get_blob(rgb_matrix::RGBMatrix *matrix, int i, float time) const;

        float calculate_field(float x, float y, const Blob &blob) const;

    public:
        explicit MetaBlobScene();

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] std::string get_name() const override;
        void register_properties() override;

        using Scene::Scene;
    };

    class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

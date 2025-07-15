#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class MetaBlobScene : public Scenes::Scene {
    private:
        PropertyPointer<int> num_blobs = MAKE_PROPERTY("num_blobs", int, 10);
        PropertyPointer<float> threshold = MAKE_PROPERTY("threshold", float, 0.0003f);
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 0.25f);
        PropertyPointer<float> move_range = MAKE_PROPERTY("move_range", float, 0.5f);
        PropertyPointer<float> color_speed = MAKE_PROPERTY("color_speed", float, 0.033f);

        float time;

        struct Blob {
            float x, y;
            float radius;

            Blob(float x, float y, float radius) : x(x), y(y), radius(radius) {
            }
        };

        std::vector<Blob> blobs;

        // Shader conversion helpers
        float rand_sin(int i) const;

        Blob get_blob(rgb_matrix::RGBMatrixBase *matrix, int i, float time) const;

        float calculate_field(float x, float y, const Blob &blob) const;

    public:
        explicit MetaBlobScene();

        ~MetaBlobScene() override = default;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }

        [[nodiscard]] std::string get_name() const override;

        void register_properties() override;

        using Scene::Scene;
    };

    class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

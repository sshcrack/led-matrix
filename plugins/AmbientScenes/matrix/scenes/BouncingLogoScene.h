#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>

namespace AmbientScenes {
    class BouncingLogoScene : public Scenes::Scene {
    private:
        float pos_x, pos_y;
        float vel_x, vel_y;
        int logo_width, logo_height;
        uint8_t color_r, color_g, color_b;

        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 0.5f);
        PropertyPointer<int> size = MAKE_PROPERTY("size", int, 16);

        void change_color();
        void draw_logo(int x, int y);

    public:
        explicit BouncingLogoScene();
        ~BouncingLogoScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class BouncingLogoSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}

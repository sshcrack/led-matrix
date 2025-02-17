#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"

using namespace Scenes;

namespace Scenes {
    class WaveScene : public Scene {
    private:
        float *map = nullptr;
        FrameTimer frameTimer;

        void drawMap(rgb_matrix::RGBMatrix *matrix, float *iMap);

    public:
        bool render(rgb_matrix::RGBMatrix *matrix) override;

        using Scene::Scene::Scene;
        ~WaveScene() override;

        void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override {}
    };


    class WaveSceneWrapper : public Plugins::SceneWrapper {
    public:

        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
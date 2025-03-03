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

        void drawMap(RGBMatrixBase *matrix, float *iMap);

    public:
        bool render(RGBMatrixBase *matrix) override;

        using Scene::Scene::Scene;
        ~WaveScene() override;

        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override {}
    };


    class WaveSceneWrapper : public Plugins::SceneWrapper {
    public:

        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
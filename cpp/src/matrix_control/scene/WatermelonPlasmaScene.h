#pragma once
#include "Scene.h"
#include "shared/utils/FrameTimer.h"

using Scenes::Scene;
namespace Scenes {
    class WatermelonPlasmaScene : Scene {
    private:
        FrameTimer frameTimer;
    public:
        bool tick(rgb_matrix::RGBMatrix *matrix) override;
        using Scene::Scene;
    };

}
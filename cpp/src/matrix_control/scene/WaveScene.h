#pragma once
#include "Scene.h"
#include "shared/utils/FrameTimer.h"


using Scenes::Scene;

namespace Scenes {
    class WaveScene : Scene {
    private:
        FrameTimer frameTimer;
        void drawMap(rgb_matrix::RGBMatrix* matrix, float *map);
    public:
        bool tick(rgb_matrix::RGBMatrix *matrix) override;
        explicit WaveScene(rgb_matrix::RGBMatrix *matrix);
    };
}
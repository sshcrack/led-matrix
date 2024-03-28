#pragma once

#include "led-matrix.h"
#include "content-streamer.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;

namespace Scenes {
    class Scene {
    protected:
        FrameCanvas* offscreen_canvas;

    public:
        explicit Scene(RGBMatrix* matrix);
        // Returns true if the scene is done and should be removed
        virtual bool tick(RGBMatrix *matrix) = 0;
    };


}
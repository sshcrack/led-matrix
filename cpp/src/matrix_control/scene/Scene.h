#pragma once

#include "led-matrix.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;

class Scene {
protected:
    FrameCanvas* offscreen_canvas;

public:
    explicit Scene(RGBMatrix* matrix);
    virtual void tick(RGBMatrix *matrix) = 0;
};


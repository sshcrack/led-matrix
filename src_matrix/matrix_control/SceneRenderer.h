#pragma once

#include <memory>

#include "led-matrix.h"
#include "shared/matrix/Scene.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class SceneRenderer {
public:
    explicit SceneRenderer(RGBMatrixBase *matrix);

    bool render_scene_phase(
        std::shared_ptr<Scenes::Scene> scene,
        FrameCanvas *&composite_offscreen_canvas,
        tmillis_t end_ms);

    void render_fallback();

private:
    RGBMatrixBase *matrix_;
};

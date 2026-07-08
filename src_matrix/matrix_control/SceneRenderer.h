#pragma once

#include <memory>

#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class SceneRenderer {
public:
    SceneRenderer(RGBMatrixBase *matrix,
                  TimeSource *time_source,
                  PostProcessor *post_processor);

    bool render_scene_phase(
        std::shared_ptr<Scenes::Scene> scene,
        FrameCanvas *&composite_offscreen_canvas,
        tmillis_t end_ms);

    void render_fallback();

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    PostProcessor *post_processor_;
};

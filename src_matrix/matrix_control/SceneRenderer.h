#pragma once

#include <atomic>
#include <memory>

#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"
#include "MatrixPresenter.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class SceneRenderer {
public:
    SceneRenderer(RGBMatrixBase *matrix,
                  TimeSource *time_source,
                  PostProcessor *post_processor,
                  MatrixPresenter *presenter,
                  const std::atomic<bool> *exit_flag,
                  const std::atomic<bool> *interrupt_flag);

    bool render_scene_phase(
        std::shared_ptr<Scenes::Scene> scene,
        FrameCanvas *&composite_offscreen_canvas,
        tmillis_t end_ms);

    void render_fallback();

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    PostProcessor *post_processor_;
    MatrixPresenter *presenter_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
};

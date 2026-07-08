#pragma once

#include <memory>

#include "led-matrix.h"
#include "shared/matrix/Scene.h"

#include "SceneScheduler.h"
#include "SceneRenderer.h"
#include "TransitionEngine.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class CanvasCoordinator {
public:
    explicit CanvasCoordinator(RGBMatrixBase *matrix);
    ~CanvasCoordinator();

    void run(std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);

private:
    RGBMatrixBase *matrix_;
    FrameCanvas *first_offscreen_canvas_ = nullptr;
    FrameCanvas *second_offscreen_canvas_ = nullptr;
    FrameCanvas *composite_offscreen_canvas_ = nullptr;
    std::shared_ptr<Scenes::Scene> forced_scene_;

    SceneScheduler scheduler_;
    SceneRenderer renderer_;
    TransitionEngine transition_engine_;
};

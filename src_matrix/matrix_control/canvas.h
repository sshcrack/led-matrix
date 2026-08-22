#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/post_processor.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/transition_manager.h"

#include "MatrixPresenter.h"
#include "AutomaticDirector.h"
#include "TransitionPlanner.h"
#include "SceneScheduler.h"
#include "SceneRenderer.h"
#include "TransitionEngine.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class CanvasCoordinator {
public:
    CanvasCoordinator(RGBMatrixBase *matrix,
                      TimeSource *time_source,
                      PostProcessor *post_processor,
                      TransitionManager *transition_manager,
                      MatrixPresenter *presenter,
                      Config::MainConfig *cfg,
                      const std::atomic<bool> *exit_flag,
                      const std::atomic<bool> *interrupt_flag,
                      std::function<RuntimeInputs::Snapshot()> runtime_inputs,
                      std::function<void(std::shared_ptr<Scenes::Scene>)> set_curr_scene,
                      std::function<void(const std::string &)> broadcast);
    ~CanvasCoordinator();

    void run(std::shared_ptr<Scenes::Scene> pinned_scene = nullptr);

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    MatrixPresenter *presenter_;
    Config::MainConfig *config_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
    FrameCanvas *first_offscreen_canvas_ = nullptr;
    FrameCanvas *second_offscreen_canvas_ = nullptr;
    FrameCanvas *composite_offscreen_canvas_ = nullptr;
    std::shared_ptr<Scenes::Scene> forced_scene_;

    std::function<RuntimeInputs::Snapshot()> runtime_inputs_fn_;
    std::function<void(std::shared_ptr<Scenes::Scene>)> set_curr_scene_fn_;
    std::function<void(const std::string &)> broadcast_fn_;

    SceneScheduler scheduler_;
    AutomaticDirector automatic_director_;
    std::uint64_t automatic_director_generation_ = 0;
    TransitionPlanner transition_planner_;
    std::vector<std::shared_ptr<Scenes::Scene>> automatic_scenes_;
    std::shared_ptr<ConfigData::Preset> automatic_preset_;

    void ensure_automatic_catalog();
    SceneRenderer renderer_;
    TransitionEngine transition_engine_;
};

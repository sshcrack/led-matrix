#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"
#include "shared/matrix/transition_manager.h"
#include "MatrixPresenter.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class TransitionEngine {
public:
    TransitionEngine(RGBMatrixBase *matrix,
                     TimeSource *time_source,
                     PostProcessor *post_processor,
                     TransitionManager *transition_manager,
                     MatrixPresenter *presenter,
                     const std::atomic<bool> *exit_flag,
                     const std::atomic<bool> *interrupt_flag);

    void render_transition_phase(
        std::shared_ptr<Scenes::Scene> scene,
        std::shared_ptr<Scenes::Scene> next_scene,
        FrameCanvas *first_offscreen_canvas,
        FrameCanvas *second_offscreen_canvas,
        FrameCanvas *&composite_offscreen_canvas,
        int matrix_width,
        int matrix_height,
        tmillis_t transition_duration,
        const std::string &transition_name,
        std::shared_ptr<Scenes::Scene> &forced_scene);

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    PostProcessor *post_processor_;
    TransitionManager *transition_manager_;
    MatrixPresenter *presenter_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;

    void apply_transition_frame(
        FrameCanvas *dst,
        FrameCanvas *from,
        FrameCanvas *to,
        float alpha_progress,
        int width,
        int height,
        const std::string &transition_name);

    void copy_canvas(FrameCanvas *dst, FrameCanvas *src, int width, int height);

    static tmillis_t render_interval_ms_from_visibility(float visibility);
};

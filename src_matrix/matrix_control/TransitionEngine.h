#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "DesktopHandoff.h"
#include "FrameSink.h"
#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"
#include "shared/matrix/transition_manager.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class TransitionEngine {
public:
    TransitionEngine(TimeSource* time_source, TransitionManager* transition_manager, FrameSink* sink, const std::atomic<bool>* exit_flag,
                     const std::atomic<bool>* interrupt_flag, DesktopHandoff* handoff = nullptr);

    void render_transition_phase(std::shared_ptr<Scenes::Scene> scene, std::shared_ptr<Scenes::Scene> next_scene,
                                 FrameCanvas* first_offscreen_canvas, FrameCanvas* second_offscreen_canvas,
                                 FrameCanvas*& composite_offscreen_canvas, int matrix_width, int matrix_height,
                                 tmillis_t transition_duration, const std::string& transition_name,
                                 std::shared_ptr<Scenes::Scene>& forced_scene, tmillis_t start_delay_ms = 0,
                                 std::function<bool()> inputs_still_available = {});

private:
    TimeSource* time_source_;
    TransitionManager *transition_manager_;
    FrameSink* sink_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
    /// Shared with SceneRenderer (see Canvas) so a warmed session carries over.
    std::unique_ptr<DesktopHandoff> own_handoff_;
    DesktopHandoff *handoff_;

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

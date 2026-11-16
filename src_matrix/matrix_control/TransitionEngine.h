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

/// What a transition phase decided for the scene that follows it.
struct TransitionResult {
    /// The incoming scene, set whenever it took over the panel (the blend ended,
    /// the desktop session was adopted, or the phase ended early with it). Null
    /// when the phase was cut short before the incoming scene took over
    /// (interrupt/exit before the blend, or a required Runtime Input vanished):
    /// the caller then picks a successor itself.
    std::shared_ptr<Scenes::Scene> forced_scene;
};

class TransitionEngine {
public:
    TransitionEngine(TimeSource* time_source, TransitionManager* transition_manager, FrameSink* sink, const std::atomic<bool>* exit_flag,
                     const std::atomic<bool>* interrupt_flag, DesktopHandoff* handoff = nullptr);

    /// Runs the optional beat delay and the transition from `scene` to `next_scene`.
    [[nodiscard]] TransitionResult render_transition_phase(std::shared_ptr<Scenes::Scene> scene, std::shared_ptr<Scenes::Scene> next_scene,
                                                           FrameCanvas* first_offscreen_canvas, FrameCanvas* second_offscreen_canvas,
                                                           FrameCanvas*& composite_offscreen_canvas, int matrix_width, int matrix_height,
                                                           tmillis_t transition_duration, const std::string& transition_name,
                                                           tmillis_t start_delay_ms = 0, std::function<bool()> inputs_still_available = {});

private:
    TimeSource* time_source_;
    TransitionManager *transition_manager_;
    FrameSink* sink_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
    /// Shared with SceneRenderer (see Canvas) so a warmed session carries over.
    std::unique_ptr<DesktopHandoff> own_handoff_;
    DesktopHandoff *handoff_;

    static tmillis_t render_interval_ms_from_visibility(float visibility);
};

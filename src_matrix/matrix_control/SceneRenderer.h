#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "DesktopHandoff.h"
#include "FrameSink.h"
#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

class SceneRenderer {
public:
    SceneRenderer(RGBMatrixBase* matrix, TimeSource* time_source, FrameSink* sink, const std::atomic<bool>* exit_flag,
                  const std::atomic<bool>* interrupt_flag, DesktopHandoff* handoff = nullptr);

    bool render_scene_phase(
        std::shared_ptr<Scenes::Scene> scene,
        FrameCanvas *&composite_offscreen_canvas,
        tmillis_t end_ms,
        std::function<bool()> inputs_still_available = {},
        std::function<bool()> switch_requested = {});

    void render_fallback();

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    FrameSink* sink_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
    /// Shared with TransitionEngine (see Canvas) so a warmed session carries over.
    std::unique_ptr<DesktopHandoff> own_handoff_;
    DesktopHandoff* handoff_;
};

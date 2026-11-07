#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <optional>

#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/post_processor.h"
#include "DesktopHandoff.h"
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
                  const std::atomic<bool> *interrupt_flag,
                  DesktopHandoff *handoff = nullptr);

    bool render_scene_phase(
        std::shared_ptr<Scenes::Scene> scene,
        FrameCanvas *&composite_offscreen_canvas,
        tmillis_t end_ms,
        std::function<bool()> inputs_still_available = {},
        std::function<bool()> switch_requested = {});

    void render_fallback();

    /// Supplies the desired panel brightness in percent, or nullopt to leave it untouched.
    /// Polled at most once per second from the render thread.
    void set_brightness_provider(std::function<std::optional<int>()> provider) { brightness_provider_ = std::move(provider); }

    /// Canvas currently latched on the matrix after the most recent successful
    /// presentation. This is only used at scene-transition boundaries, avoiding
    /// a per-frame framebuffer copy just to recover the visible outgoing frame.
    [[nodiscard]] FrameCanvas *last_presented_canvas() const { return last_presented_canvas_; }

private:
    RGBMatrixBase *matrix_;
    TimeSource *time_source_;
    PostProcessor *post_processor_;
    MatrixPresenter *presenter_;
    const std::atomic<bool> *exit_flag_;
    const std::atomic<bool> *interrupt_flag_;
    /// Shared with TransitionEngine (see Canvas) so a warmed session carries over.
    std::unique_ptr<DesktopHandoff> own_handoff_;
    DesktopHandoff *handoff_;
    FrameCanvas *last_presented_canvas_ = nullptr;
    std::function<std::optional<int>()> brightness_provider_;
    tmillis_t next_brightness_check_ms_ = 0;
    /// Present while a brightness schedule controls the panel. `restore_to`
    /// is the brightness from before the takeover, restored when it ends.
    struct BrightnessTakeover {
        int applied = 0;
        int restore_to = 0;
    };
    std::optional<BrightnessTakeover> brightness_takeover_;

    void apply_scheduled_brightness(tmillis_t now_ms);
};

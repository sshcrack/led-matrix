#pragma once

#include <functional>
#include <optional>

#include "MatrixPresenter.h"
#include "led-matrix.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/post_processor.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;

/// The one place a composed frame becomes the frame on the panel.
///
/// `present()` applies post-effects, serves a pending live-preview request,
/// applies the scheduled brightness, swaps the canvas onto the matrix, calls
/// the presenter hook and records presentation diagnostics. SceneRenderer and
/// TransitionEngine both present through it, so they cannot drift apart, and
/// the frame on the panel (`visible()`) is tracked in the same place it changes.
///
/// Render thread only. Live capture stays pull-based: without an outstanding
/// viewer request `present()` reads no pixels and takes no lock.
class FrameSink {
public:
    FrameSink(RGBMatrixBase* matrix, TimeSource* time_source, PostProcessor* post_processor, MatrixPresenter* presenter);

    /// Supplies the desired panel brightness in percent, or nullopt to leave it untouched.
    /// Polled at most once per second from the render thread.
    void set_brightness_provider(std::function<std::optional<int>()> provider) { brightness_provider_ = std::move(provider); }

    /// Presents `canvas`. On return `canvas` is the buffer the caller renders into next.
    void present(FrameCanvas*& canvas, int target_fps);

    /// Canvas latched on the matrix by the most recent present(), nullptr before the first one.
    /// Valid until the next present(); read at scene-transition boundaries to recover the
    /// outgoing frame without a per-frame framebuffer copy.
    [[nodiscard]] FrameCanvas* visible() const { return visible_; }

private:
    RGBMatrixBase* matrix_;
    TimeSource* time_source_;
    PostProcessor* post_processor_;
    MatrixPresenter* presenter_;
    FrameCanvas* visible_ = nullptr;
    std::function<std::optional<int>()> brightness_provider_;
    tmillis_t next_brightness_check_ms_ = 0;
    /// Present while a brightness schedule controls the panel. `restore_to`
    /// is the brightness from before the takeover, restored when it ends.
    struct BrightnessTakeover {
        int applied = 0;
        int restore_to = 0;
    };
    std::optional<BrightnessTakeover> brightness_takeover_;

    void apply_scheduled_brightness();
};

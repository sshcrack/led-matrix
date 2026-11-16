#include "FrameSink.h"

#include <algorithm>

#include "LiveFrameSnapshot.h"
#include "shared/matrix/diagnostics.h"
#include "spdlog/spdlog.h"

FrameSink::FrameSink(RGBMatrixBase* matrix, TimeSource* time_source, PostProcessor* post_processor, MatrixPresenter* presenter)
    : matrix_(matrix), time_source_(time_source), post_processor_(post_processor), presenter_(presenter)
{
}

void FrameSink::present(FrameCanvas*& canvas, const int target_fps)
{
    if (post_processor_)
        post_processor_->apply_effects(canvas);

    LiveFrame::SnapshotStore::instance().capture_if_requested(canvas, matrix_->width(), matrix_->height());

    apply_scheduled_brightness();
    FrameCanvas* const shown = canvas;
    canvas = matrix_->SwapOnVSync(canvas, 1);
    visible_ = shown;
    presenter_->present();
    Diagnostics::RuntimeDiagnostics::instance().record_presentation(target_fps);
}

void FrameSink::apply_scheduled_brightness()
{
    if (!brightness_provider_)
        return;
    const auto now_ms = time_source_->now_ms();
    if (now_ms < next_brightness_check_ms_)
        return;
    next_brightness_check_ms_ = now_ms + 1000;

    const auto desired = brightness_provider_();
    if (!desired.has_value()) {
        // Schedule switched off: restore the brightness the matrix had before we took over (--led-brightness).
        if (brightness_takeover_ && brightness_takeover_->restore_to > 0) {
            matrix_->SetBrightness(static_cast<uint8_t>(brightness_takeover_->restore_to));
            spdlog::debug("Brightness schedule disabled, restored {}%", brightness_takeover_->restore_to);
        }
        brightness_takeover_.reset();
        return;
    }
    const int brightness = std::clamp(*desired, 1, 100);
    if (brightness_takeover_ && brightness == brightness_takeover_->applied)
        return;
    if (!brightness_takeover_)
        brightness_takeover_ = BrightnessTakeover{.applied = 0, .restore_to = matrix_->brightness()};

    matrix_->SetBrightness(static_cast<uint8_t>(brightness));
    brightness_takeover_->applied = brightness;
    spdlog::debug("Scheduled brightness set to {}%", brightness);
}

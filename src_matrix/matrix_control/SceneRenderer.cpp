#include "SceneRenderer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "shared/common/crash_reporter.h"
#include "shared/common/frame_cadence.h"
#include "shared/matrix/diagnostics.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"

using namespace spdlog;

SceneRenderer::SceneRenderer(RGBMatrixBase* matrix, TimeSource* time_source, FrameSink* sink, const std::atomic<bool>* exit_flag,
                             const std::atomic<bool>* interrupt_flag, DesktopHandoff* handoff)
    : matrix_(matrix),
      time_source_(time_source),
      sink_(sink),
      exit_flag_(exit_flag),
      interrupt_flag_(interrupt_flag),
      own_handoff_(handoff ? nullptr : std::make_unique<DesktopHandoff>()),
      handoff_(handoff ? handoff : own_handoff_.get())
{
}

bool SceneRenderer::render_scene_phase(
    std::shared_ptr<Scenes::Scene> scene,
    FrameCanvas *&composite_offscreen_canvas,
    tmillis_t end_ms,
    std::function<bool()> inputs_still_available,
    std::function<bool()> switch_requested)
{
    auto &diagnostics = Diagnostics::RuntimeDiagnostics::instance();
    diagnostics.set_active_scene(scene->get_name());
    diagnostics.reset_presentation_cadence();

    handoff_->begin_scene(*scene, matrix_->width(), matrix_->height());

    tmillis_t next_input_check_ms = time_source_->now_ms();
    tmillis_t next_switch_check_ms = next_input_check_ms;
    FrameCadence cadence(time_source_->now_us(), scene->get_declared_target_fps());

    while (time_source_->now_ms() < end_ms) {
        // VSync already consumes part (or all) of the frame budget. Wait only
        // for the remainder of the absolute deadline, in cancellable slices.
        while (time_source_->now_us() < cadence.deadline_us()) {
            if (*exit_flag_ || *interrupt_flag_ || exit_canvas_update || skip_image.exchange(false))
                return true;
            time_source_->sleep_us(std::min<std::int64_t>(10000,
                cadence.deadline_us() - time_source_->now_us()));
        }
        if (*exit_flag_ || *interrupt_flag_ || exit_canvas_update || skip_image.exchange(false))
            return true;
        const auto now_ms = time_source_->now_ms();
        if (now_ms >= end_ms) break;
        cadence.frame_started(time_source_->now_us());
        if (inputs_still_available && now_ms >= next_input_check_ms) {
            next_input_check_ms = now_ms + 250;
            if (!inputs_still_available()) {
                spdlog::debug("Scene '{}' lost a required Runtime Input; selecting a replacement", scene->get_name());
                return true;
            }
        }
        if (switch_requested && now_ms >= next_switch_check_ms) {
            next_switch_check_ms = now_ms + 250;
            if (switch_requested()) {
                spdlog::debug("Automatic Director requested an early handoff from '{}'", scene->get_name());
                return true;
            }
        }
        bool cont = true;
        bool local_frame_updated = false;
        const auto source = handoff_->scene_frame(composite_offscreen_canvas);
        const bool used_remote_frame = source == DesktopHandoff::FrameSource::Remote;
        const bool render_local = source == DesktopHandoff::FrameSource::Local;

        if (render_local) {
            const auto render_start = std::chrono::steady_clock::now();
            try {
                cont = scene->render_frame(composite_offscreen_canvas, std::nullopt, true);
                local_frame_updated = scene->frame_was_updated();
            } catch (const std::exception &e) {
                diagnostics.record_scene_error(scene->get_name(), e.what());
                spdlog::error("Scene '{}' threw while rendering: {}", scene->get_name(), e.what());
                composite_offscreen_canvas->Clear();
                return true;
            } catch (...) {
                diagnostics.record_scene_error(scene->get_name(), "unknown exception");
                spdlog::error("Scene '{}' threw an unknown exception while rendering", scene->get_name());
                composite_offscreen_canvas->Clear();
                return true;
            }
            const double wall_render_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - render_start).count();
            const double active_render_ms = std::max(0.0, wall_render_ms - scene->get_last_frame_wait_ms());
            scene->report_render_cost(active_render_ms);
            diagnostics.record_render(
                scene->get_name(), active_render_ms, scene->get_declared_target_fps(),
                scene->get_render_quality_scale());

            handoff_->local_frame_rendered(active_render_ms);
        }

        const bool frame_updated = used_remote_frame || local_frame_updated;
        if (!frame_updated)
            diagnostics.reset_presentation_cadence();
        if (frame_updated) {
            sink_->present(composite_offscreen_canvas, scene->get_declared_target_fps());
        }

        if (!cont || *interrupt_flag_ || *exit_flag_) {
            trace("Exiting scene early.");
            return true;
        }
    }

    return false;
}

void SceneRenderer::render_fallback()
{
    static rgb_matrix::Color ERROR_COLOR(255, 0, 0);
    static rgb_matrix::Font ERROR_FONT;
    static bool load_font_error = false;
    static bool loaded = false;

    if (!loaded && !load_font_error) {
#ifndef LED_MATRIX_SHARE_DIR
        constexpr const char *kFontDir = ".";
#else
        constexpr const char *kFontDir = LED_MATRIX_SHARE_DIR;
#endif
        if (!ERROR_FONT.LoadFont(
                (std::filesystem::path(kFontDir) / "7x13.bdf").c_str())) {
            spdlog::error("Could not load error font");
            load_font_error = true;
            return;
        }
        loaded = true;
    }

    if (load_font_error) {
        spdlog::error("Error font not loaded, cannot render fallback");
        return;
    }

    matrix_->Fill(0, 0, 0);
    rgb_matrix::DrawText(matrix_, ERROR_FONT, 0, 11, ERROR_COLOR, "No scene available");
}

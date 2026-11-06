#include "SceneRenderer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "LiveFrameSnapshot.h"
#include "RenderPlacement.h"
#include "shared/common/crash_reporter.h"
#include "shared/matrix/diagnostics.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/remote_render.h"
#include "shared/matrix/runtime_inputs.h"
#include "spdlog/spdlog.h"

using namespace spdlog;

SceneRenderer::SceneRenderer(RGBMatrixBase *matrix,
                              TimeSource *time_source,
                              PostProcessor *post_processor,
                              MatrixPresenter *presenter,
                              const std::atomic<bool> *exit_flag,
                              const std::atomic<bool> *interrupt_flag)
    : matrix_(matrix)
    , time_source_(time_source)
    , post_processor_(post_processor)
    , presenter_(presenter)
    , exit_flag_(exit_flag)
    , interrupt_flag_(interrupt_flag)
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

    const auto caps = scene->get_capabilities();
    const double budget_ms = 1000.0 / static_cast<double>(
        std::max(1, scene->get_declared_target_fps()));
    const auto desktop_available = [&scene] {
        return RemoteRender::worker_available(scene->get_name());
    };

    RenderPlacement placement;
    const auto request_desktop = [&] {
        placement.request_desktop(RemoteRender::request_scene(
            *scene, matrix_->width(), matrix_->height(), scene->get_declared_target_fps()));
    };
    const auto report_placement = [&](const std::string &reason, nlohmann::json extra = nlohmann::json::object()) {
        const auto mode = placement.mode();
        const char *activity = mode == RenderPlacement::Mode::DesktopLive ? "desktop"
            : mode == RenderPlacement::Mode::DesktopPending ? "desktop pending"
            : mode == RenderPlacement::Mode::LocalFallback ? "local fallback" : "local";
        CrashReporter::set_activity(std::string("rendering scene '") + scene->get_name() + "' (" + activity + ")");
        nlohmann::json payload{
            {"scene", scene->get_name()},
            {"placement", RenderPlacement::label(mode)},
            {"reason", reason},
            {"frame_budget_ms", budget_ms},
            {"quality_scale", mode == RenderPlacement::Mode::DesktopLive ? 1.0f : scene->get_render_quality_scale()},
        };
        payload.update(extra);
        diagnostics.set_render_placement(std::move(payload));
    };
    std::string placement_reason = "local render is within the measured frame budget";
    if (caps.supports_remote_rendering && desktop_available()) {
        const auto remote_status = RemoteRender::status();
        if (remote_status.requested && remote_status.scene == scene->get_name()) {
            // A transition may already have warmed the incoming scene on the
            // desktop. request_scene() reuses the session when the UUID matches.
            request_desktop();
            placement_reason = "desktop session was warmed during the transition";
        } else if (scene->get_render_quality_scale() < 0.97f) {
            request_desktop();
            placement_reason = "Pi renders this scene at reduced quality; the desktop renders it at full quality";
        } else if (const auto p95 = diagnostics.scene_render_p95(scene->get_name());
                   p95.has_value() && *p95 > budget_ms * RenderPlacement::PressureBudgetFraction) {
            request_desktop();
            placement_reason = "measured local p95 exceeds 72% of the frame budget";
        }
    } else {
        const auto remote_status = RemoteRender::status();
        if (remote_status.requested)
            RemoteRender::stop();
        if (!caps.supports_remote_rendering)
            placement_reason = "scene explicitly opted out of generic desktop execution; adaptive local quality is active";
        else if (remote_status.worker_available && !remote_status.worker_compatible)
            placement_reason = "desktop scene worker runs different scene code (update the desktop app to this version); rendering locally";
        else
            placement_reason = "desktop scene worker is unavailable; rendering locally";
    }

    report_placement(placement_reason);

    tmillis_t next_input_check_ms = time_source_->now_ms();
    tmillis_t next_switch_check_ms = next_input_check_ms;
    // Pace from the previous render start, not from the previous completed
    // SwapOnVSync(). SwapOnVSync() already waits for the next matrix refresh.
    // Pacing from the completed swap adds a second full frame interval at
    // 60 FPS (sleep ~16 ms, then wait for VSync again), which can reduce local
    // presentation to roughly half rate on a healthy renderer.
    tmillis_t last_frame_start_ms = 0;
    const tmillis_t target_step_ms = std::max<tmillis_t>(
        1, 1000 / std::max(1, scene->get_declared_target_fps()));

    while (time_source_->now_ms() < end_ms) {
        auto now_ms = time_source_->now_ms();
        // SwapOnVSync below supplies the unavoidable hardware VSync wait. This
        // time-based limiter only fills the remainder for scenes targeting a
        // lower update rate. Anchoring it to render start means a 60 FPS scene
        // normally reaches this point after its target interval has already
        // elapsed while waiting for the previous VSync, so no second sleep is
        // introduced.
        if (last_frame_start_ms != 0 && now_ms < last_frame_start_ms + target_step_ms) {
            SleepMillis(last_frame_start_ms + target_step_ms - now_ms);
            now_ms = time_source_->now_ms();
        }
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
        const tmillis_t frame_start_ms = time_source_->now_ms();

        bool cont = true;
        bool used_remote_frame = false;
        bool local_frame_updated = false;
        if (placement.has_session() && !desktop_available()) {
            placement.worker_lost();
            report_placement("desktop scene worker became unavailable; immediate local fallback");
        }
        if (const auto session = placement.session()) {
            RemoteRender::publish_runtime_state(*session);
            used_remote_frame = RemoteRender::copy_latest(
                *session, composite_offscreen_canvas, matrix_->width(), matrix_->height());
            if (used_remote_frame && placement.remote_frame_presented())
                report_placement(placement_reason);
        }

        bool render_local = !used_remote_frame;
        if (render_local && placement.mode() == RenderPlacement::Mode::DesktopLive) {
            const auto session = *placement.session();
            const auto decision = placement.no_remote_frame(RemoteRender::latest_frame_age_ms(session));
            render_local = decision == RenderPlacement::NoRemoteFrame::RenderLocal;
            if (render_local)
                report_placement("remote frame became stale; continuing locally without blocking");
        }

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

            // Learn on the actual Pi instead of guessing from x86/QEMU timings.
            // Sustained pressure starts the desktop in parallel; local
            // rendering continues until the first complete remote frame
            // arrives, so there is never a blank handoff.
            if (placement.local_frame_rendered(active_render_ms, budget_ms)
                && caps.supports_remote_rendering && desktop_available()) {
                request_desktop();
                if (placement.has_session()) {
                    placement_reason = "live Pi render cost exceeded 72% of the frame budget for four frames";
                    report_placement(placement_reason, {{"local_render_ms", active_render_ms}});
                }
            }
        }

        const bool frame_updated = used_remote_frame || local_frame_updated;
        if (!frame_updated)
            diagnostics.reset_presentation_cadence();
        if (frame_updated) {
            if (post_processor_)
                post_processor_->apply_effects(composite_offscreen_canvas);

            LiveFrame::SnapshotStore::instance().capture_if_requested(
                composite_offscreen_canvas, matrix_->width(), matrix_->height());

            apply_scheduled_brightness(frame_start_ms);
            auto *presented_canvas = composite_offscreen_canvas;
            composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);
            last_presented_canvas_ = presented_canvas;
            presenter_->present();
            diagnostics.record_presentation(scene->get_declared_target_fps());
        }

        // A held frame skips SwapOnVSync, so retain software pacing instead of
        // busy-looping while the scene waits for new state.
        last_frame_start_ms = frame_start_ms;

        if (!cont || *interrupt_flag_ || *exit_flag_) {
            trace("Exiting scene early.");
            return true;
        }
    }

    return false;
}

void SceneRenderer::apply_scheduled_brightness(const tmillis_t now_ms)
{
    if (!brightness_provider_ || now_ms < next_brightness_check_ms_) return;
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
    if (brightness_takeover_ && brightness == brightness_takeover_->applied) return;
    if (!brightness_takeover_) brightness_takeover_ = BrightnessTakeover{.applied = 0, .restore_to = matrix_->brightness()};

    matrix_->SetBrightness(static_cast<uint8_t>(brightness));
    brightness_takeover_->applied = brightness;
    spdlog::debug("Scheduled brightness set to {}%", brightness);
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

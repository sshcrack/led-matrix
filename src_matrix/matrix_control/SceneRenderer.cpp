#include "SceneRenderer.h"

#include <filesystem>

#include "spdlog/spdlog.h"
#include "shared/matrix/diagnostics.h"
#include <chrono>

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
    tmillis_t end_ms)
{
    Diagnostics::RuntimeDiagnostics::instance().set_active_scene(scene->get_name());
    while (time_source_->now_ms() < end_ms) {
        bool cont = false;
        const auto render_start = std::chrono::steady_clock::now();
        try {
            cont = scene->render_frame(composite_offscreen_canvas);
        } catch (const std::exception &e) {
            Diagnostics::RuntimeDiagnostics::instance().record_scene_error(scene->get_name(), e.what());
            spdlog::error("Scene '{}' threw while rendering: {}", scene->get_name(), e.what());
            composite_offscreen_canvas->Clear();
            return true;
        } catch (...) {
            Diagnostics::RuntimeDiagnostics::instance().record_scene_error(scene->get_name(), "unknown exception");
            spdlog::error("Scene '{}' threw an unknown exception while rendering", scene->get_name());
            composite_offscreen_canvas->Clear();
            return true;
        }
        const double wall_render_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - render_start).count();
        const double active_render_ms = std::max(0.0, wall_render_ms - scene->get_last_frame_wait_ms());
        scene->report_render_cost(active_render_ms);
        Diagnostics::RuntimeDiagnostics::instance().record_render(
            scene->get_name(), active_render_ms, scene->get_declared_target_fps(),
            scene->get_render_quality_scale());

        if (post_processor_)
            post_processor_->apply_effects(composite_offscreen_canvas);

        composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);

        presenter_->present();

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

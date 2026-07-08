#include "SceneRenderer.h"

#include <filesystem>

#include "spdlog/spdlog.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

using namespace spdlog;

SceneRenderer::SceneRenderer(RGBMatrixBase *matrix,
                              TimeSource *time_source,
                              PostProcessor *post_processor)
    : matrix_(matrix)
    , time_source_(time_source)
    , post_processor_(post_processor)
{
}

bool SceneRenderer::render_scene_phase(
    std::shared_ptr<Scenes::Scene> scene,
    FrameCanvas *&composite_offscreen_canvas,
    tmillis_t end_ms)
{
    while (time_source_->now_ms() < end_ms) {
        bool cont = scene->render(composite_offscreen_canvas);

        if (post_processor_)
            post_processor_->apply_effects(composite_offscreen_canvas);

        composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);

#ifdef ENABLE_EMULATOR
        static_cast<rgb_matrix::EmulatorMatrix *>(matrix_)->Render();
#endif

        if (!cont || interrupt_received || exit_canvas_update) {
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

#include "TransitionEngine.h"

#include "spdlog/spdlog.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/utils/utils.h"

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

using namespace spdlog;

TransitionEngine::TransitionEngine(RGBMatrixBase *matrix)
    : matrix_(matrix)
{
}

void TransitionEngine::copy_canvas(FrameCanvas *dst, FrameCanvas *src, int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            src->GetPixel(x, y, &r, &g, &b);
            dst->SetPixel(x, y, r, g, b);
        }
    }
}

void TransitionEngine::apply_transition_frame(
    FrameCanvas *dst,
    FrameCanvas *from,
    FrameCanvas *to,
    float alpha_progress,
    int width,
    int height,
    const std::string &transition_name)
{
    TransitionEffect *transition_effect = nullptr;
    if (auto *tm = Constants::global_transition_manager) {
        transition_effect = tm->get_transition(transition_name);
        if (transition_effect == nullptr)
            transition_effect = tm->get_transition("blend");
    }

    if (transition_effect != nullptr) {
        transition_effect->apply(dst, from, to, alpha_progress, width, height);
        return;
    }

    copy_canvas(dst, to, width, height);
}

tmillis_t TransitionEngine::render_interval_ms_from_visibility(float visibility)
{
    const auto clamped = std::clamp(visibility, 0.0f, 1.0f);
    constexpr tmillis_t min_interval_ms = 33;
    constexpr tmillis_t max_interval_ms = 140;
    const auto range = max_interval_ms - min_interval_ms;
    const auto interval = max_interval_ms -
        static_cast<tmillis_t>(clamped * static_cast<float>(range));
    return std::clamp(interval, min_interval_ms, max_interval_ms);
}

void TransitionEngine::render_transition_phase(
    std::shared_ptr<Scenes::Scene> scene,
    std::shared_ptr<Scenes::Scene> next_scene,
    FrameCanvas *first_offscreen_canvas,
    FrameCanvas *second_offscreen_canvas,
    FrameCanvas *&composite_offscreen_canvas,
    int matrix_width,
    int matrix_height,
    tmillis_t transition_duration,
    const std::string &transition_name,
    std::shared_ptr<Scenes::Scene> &forced_scene)
{
    scene->before_transition_stop();

    constexpr tmillis_t max_transition_ms = 10000;
    tmillis_t transition_start_ms = GetTimeInMillis();
    tmillis_t last_current_render_ms = transition_start_ms;
    tmillis_t last_next_render_ms = transition_start_ms;

    auto current_continue = scene->render(first_offscreen_canvas);
    auto next_continue = next_scene->render(second_offscreen_canvas);

    while (true) {
        const auto now_ms = GetTimeInMillis();
        if (now_ms - transition_start_ms > max_transition_ms) {
            apply_transition_frame(composite_offscreen_canvas,
                                   first_offscreen_canvas,
                                   second_offscreen_canvas,
                                   1.0f,
                                   matrix_width,
                                   matrix_height,
                                   transition_name);
            forced_scene = next_scene;
            break;
        }

        const auto elapsed = now_ms - transition_start_ms;
        const auto alpha = std::clamp(
            static_cast<float>(elapsed) /
                static_cast<float>(std::max<tmillis_t>(1, transition_duration)),
            0.0f, 1.0f);

        const auto current_visibility = 1.0f - alpha;
        const auto next_visibility = alpha;

        if ((now_ms - last_current_render_ms) >= render_interval_ms_from_visibility(current_visibility)) {
            current_continue = scene->render(first_offscreen_canvas);
            last_current_render_ms = now_ms;
        }

        if ((now_ms - last_next_render_ms) >= render_interval_ms_from_visibility(next_visibility)) {
            next_continue = next_scene->render(second_offscreen_canvas);
            last_next_render_ms = now_ms;
        }

        if (!current_continue || !next_continue || interrupt_received || exit_canvas_update) {
            trace("Exiting scene early.");
            forced_scene = next_scene;
            break;
        }

        apply_transition_frame(composite_offscreen_canvas,
                               first_offscreen_canvas,
                               second_offscreen_canvas,
                               alpha,
                               matrix_width,
                               matrix_height,
                               transition_name);

        if (auto *pp = Constants::global_post_processor)
            pp->apply_effects(composite_offscreen_canvas);

        composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);

#ifdef ENABLE_EMULATOR
        static_cast<rgb_matrix::EmulatorMatrix *>(matrix_)->Render();
#endif

        if (alpha >= 1.0f) {
            forced_scene = next_scene;
            break;
        }
    }
}

#include "TransitionEngine.h"
#include <vector>

#include "LiveFrameSnapshot.h"
#include "shared/common/crash_reporter.h"
#include "shared/matrix/diagnostics.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/remote_render.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"

using namespace spdlog;

TransitionEngine::TransitionEngine(RGBMatrixBase* matrix, TimeSource* time_source, PostProcessor* post_processor,
                                   TransitionManager* transition_manager, MatrixPresenter* presenter, const std::atomic<bool>* exit_flag,
                                   const std::atomic<bool>* interrupt_flag)
    : matrix_(matrix),
      time_source_(time_source),
      post_processor_(post_processor),
      transition_manager_(transition_manager),
      presenter_(presenter),
      exit_flag_(exit_flag),
      interrupt_flag_(interrupt_flag)
{
}

void TransitionEngine::copy_canvas(FrameCanvas* dst, FrameCanvas* src, int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            src->GetPixel(x, y, &r, &g, &b);
            dst->SetPixel(x, y, r, g, b);
        }
    }
}

void TransitionEngine::apply_transition_frame(FrameCanvas* dst, FrameCanvas* from, FrameCanvas* to, float alpha_progress, int width,
                                              int height, const std::string& transition_name)
{
    TransitionEffect* transition_effect = nullptr;
    if (transition_manager_) {
        transition_effect = transition_manager_->get_transition(transition_name);
        if (transition_effect == nullptr)
            transition_effect = transition_manager_->get_transition("blend");
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
    constexpr tmillis_t min_interval_ms = 16;
    constexpr tmillis_t max_interval_ms = 100;
    const auto range = max_interval_ms - min_interval_ms;
    const auto interval = max_interval_ms - static_cast<tmillis_t>(clamped * static_cast<float>(range));
    return std::clamp(interval, min_interval_ms, max_interval_ms);
}

void TransitionEngine::render_transition_phase(std::shared_ptr<Scenes::Scene> scene, std::shared_ptr<Scenes::Scene> next_scene,
                                               FrameCanvas* first_offscreen_canvas, FrameCanvas* second_offscreen_canvas,
                                               FrameCanvas*& composite_offscreen_canvas, int matrix_width, int matrix_height,
                                               tmillis_t transition_duration, const std::string& transition_name,
                                               std::shared_ptr<Scenes::Scene>& forced_scene, tmillis_t start_delay_ms,
                                               std::function<bool()> inputs_still_available,
                                               FrameCanvas *current_display_canvas)
{
    CrashReporter::set_activity(std::string("transition '") + transition_name + "': '"
        + scene->get_name() + "' -> '" + next_scene->get_name() + "'");
    tmillis_t next_input_check_ms = time_source_->now_ms();
    const auto runtime_inputs_valid = [&]() {
        if (!inputs_still_available)
            return true;
        const auto now_ms = time_source_->now_ms();
        if (now_ms < next_input_check_ms)
            return true;
        next_input_check_ms = now_ms + 250;
        if (inputs_still_available())
            return true;
        spdlog::debug("Transition aborted because a required Runtime Input disappeared");
        forced_scene.reset();
        return false;
    };

    if (start_delay_ms > 0) {
        const auto hold_start = time_source_->now_ms();
        while (time_source_->now_ms() - hold_start < start_delay_ms) {
            if (*interrupt_flag_ || *exit_flag_ || !runtime_inputs_valid())
                return;
            bool frame_updated = false;
            try {
                const auto remote_status = RemoteRender::status();
                if (remote_status.requested && remote_status.scene == scene->get_name()) {
                    RemoteRender::publish_runtime_state(remote_status.session);
                    frame_updated = RemoteRender::copy_latest(
                        remote_status.session, composite_offscreen_canvas,
                        matrix_width, matrix_height);
                }
                if (!frame_updated) {
                    if (!scene->render_frame(composite_offscreen_canvas, std::nullopt, true))
                        break;
                    frame_updated = scene->frame_was_updated();
                }
            }
            catch (...) {
                break;
            }
            if (!frame_updated) {
                // The scene deliberately held its current frame. Do not rotate
                // an untouched historical off-screen buffer onto the matrix.
                SleepMillis(std::max<tmillis_t>(1, 1000 / std::max(1, scene->get_declared_target_fps())));
                continue;
            }
            if (post_processor_)
                post_processor_->apply_effects(composite_offscreen_canvas);
            LiveFrame::SnapshotStore::instance().capture_if_requested(composite_offscreen_canvas, matrix_width, matrix_height);
            composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);
            presenter_->present();
            Diagnostics::RuntimeDiagnostics::instance().record_presentation(
                scene->get_declared_target_fps());
        }
    }

    scene->before_transition_stop();

    constexpr tmillis_t max_transition_ms = 10000;
    tmillis_t transition_start_ms = time_source_->now_ms();
    tmillis_t last_next_render_ms = transition_start_ms;

    auto safe_render = [](const std::shared_ptr<Scenes::Scene>& candidate, FrameCanvas* canvas) {
        try {
            return candidate->render_frame(canvas, std::nullopt, true);
        }
        catch (const std::exception& e) {
            Diagnostics::RuntimeDiagnostics::instance().record_scene_error(candidate->get_name(), e.what());
            spdlog::error("Scene '{}' threw during transition: {}", candidate->get_name(), e.what());
            canvas->Clear();
            return false;
        }
        catch (...) {
            Diagnostics::RuntimeDiagnostics::instance().record_scene_error(candidate->get_name(), "unknown exception");
            spdlog::error("Scene '{}' threw an unknown exception during transition", candidate->get_name());
            canvas->Clear();
            return false;
        }
    };

    if (!runtime_inputs_valid())
        return;

    // Snapshot the outgoing frame before changing any remote session. This is
    // especially important when the current scene is already desktop-rendered:
    // its local simulation may have intentionally stopped advancing.
    bool current_continue = true;
    bool current_frame_updated = false;
    const auto current_remote = RemoteRender::status();
    if (current_remote.requested && current_remote.scene == scene->get_name()) {
        current_frame_updated = RemoteRender::copy_latest(
            current_remote.session, first_offscreen_canvas, matrix_width, matrix_height);
    }
    if (!current_frame_updated) {
        current_continue = safe_render(scene, first_offscreen_canvas);
        current_frame_updated = scene->frame_was_updated();
    }
    if (!current_frame_updated && current_display_canvas != nullptr) {
        // The outgoing scene has no new pixels. Snapshot the canvas that the
        // renderer most recently latched on the matrix instead of exposing
        // arbitrary contents left in this reusable transition buffer. This
        // copy happens only at the transition boundary, not every frame.
        first_offscreen_canvas->CopyFrom(*current_display_canvas);
        current_frame_updated = true;
    }

    // Send one outgoing RGB snapshot. The worker owns transition timing and
    // composition; the Pi only writes the finished streamed pixels to GPIO.
    if (next_scene->get_capabilities().supports_remote_rendering
        && RemoteRender::worker_available(next_scene->get_name())) {
        std::vector<std::uint8_t> outgoing(static_cast<std::size_t>(matrix_width * matrix_height * 3));
        for (int y = 0; y < matrix_height; ++y)
            for (int x = 0; x < matrix_width; ++x) {
                const auto offset = static_cast<std::size_t>((y * matrix_width + x) * 3);
                first_offscreen_canvas->GetPixel(x, y, &outgoing[offset], &outgoing[offset + 1], &outgoing[offset + 2]);
            }
        const auto session = RemoteRender::request_scene(
            *next_scene, matrix_width, matrix_height, 60,
            {{"name", transition_name}, {"duration_ms", std::clamp<tmillis_t>(transition_duration, 1, max_transition_ms)},
             {"from", outgoing}});
        if (session) {
            const auto requested_at = time_source_->now_ms();
            auto last_frame_at = requested_at;
            bool presented = false;
            std::uint64_t presented_frames = 0;
            tmillis_t first_frame_at = 0;
            std::uint32_t last_sequence = 0;
            while (!*interrupt_flag_ && !*exit_flag_) {
                if (!runtime_inputs_valid()) return;
                const auto now = time_source_->now_ms();
                RemoteRender::publish_runtime_state(*session);
                const auto remote = RemoteRender::status();
                if (remote.session == *session && remote.last_sequence != last_sequence
                    && RemoteRender::copy_latest(*session, composite_offscreen_canvas,
                                                 matrix_width, matrix_height, 150.0)) {
                    last_sequence = remote.last_sequence;
                    last_frame_at = now;
                    if (!presented) first_frame_at = now;
                    presented = true;
                    ++presented_frames;
                    if (post_processor_) post_processor_->apply_effects(composite_offscreen_canvas);
                    LiveFrame::SnapshotStore::instance().capture_if_requested(
                        composite_offscreen_canvas, matrix_width, matrix_height);
                    composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);
                    presenter_->present();
                    Diagnostics::RuntimeDiagnostics::instance().record_presentation(60);
                } else {
                    SleepMillis(1);
                }
                if (presented && now - requested_at >= std::clamp<tmillis_t>(transition_duration, 1, max_transition_ms)) {
                    const auto elapsed_ms = now - first_frame_at;
                    Diagnostics::RuntimeDiagnostics::instance().set_transition_state({
                        {"name", transition_name}, {"from", scene->get_name()}, {"to", next_scene->get_name()},
                        {"placement", "desktop"}, {"frames", presented_frames},
                        {"startup_ms", first_frame_at - requested_at},
                        {"presentation_fps", elapsed_ms > 0 && presented_frames > 1
                            ? 1000.0 * static_cast<double>(presented_frames - 1) / static_cast<double>(elapsed_ms) : 0.0}});
                    forced_scene = next_scene;
                    return; // Keep the same session for ordinary scene rendering.
                }
                if (now - last_frame_at > (presented ? 150 : 500)) {
                    RemoteRender::stop();
                    if (presented) {
                        // Do not replay a half-finished handoff on disconnect.
                        forced_scene = next_scene;
                        return;
                    }
                    break; // Worker never produced a frame: use the local path.
                }
            }
            if (*interrupt_flag_ || *exit_flag_)
                return;
        }
    }

    std::optional<std::uint32_t> next_remote_session;
    const auto next_caps = next_scene->get_capabilities();
    const bool desktop_available = RemoteRender::worker_available(next_scene->get_name());
    const double next_budget_ms = 1000.0 / static_cast<double>(
        std::max(1, next_scene->get_declared_target_fps()));
    if (next_caps.supports_remote_rendering && desktop_available) {
        const auto p95 = Diagnostics::RuntimeDiagnostics::instance().scene_render_p95(next_scene->get_name());
        // Offload when the Pi is over budget or had to lower the scene's
        // quality (the desktop always renders full quality).
        if (next_scene->get_render_quality_scale() < 0.97f || (p95.has_value() && *p95 > next_budget_ms * 0.72)) {
            next_remote_session = RemoteRender::request_scene(
                *next_scene, matrix_width, matrix_height, next_scene->get_declared_target_fps());
        }
    }
    if (!next_remote_session.has_value() && current_remote.requested)
        RemoteRender::stop();

    bool have_next_frame = false;
    const auto render_next = [&]() {
        if (next_remote_session.has_value()) {
            RemoteRender::publish_runtime_state(*next_remote_session);
            if (RemoteRender::copy_latest(
                    *next_remote_session, second_offscreen_canvas,
                    matrix_width, matrix_height)) {
                have_next_frame = true;
                return true;
            }
        }

        const bool keep_running = safe_render(next_scene, second_offscreen_canvas);
        if (next_scene->frame_was_updated()) {
            have_next_frame = true;
        } else if (!have_next_frame) {
            // A not-yet-ready incoming scene (paused media, async artwork, etc.)
            // must not expose whatever pixels happened to be left in this
            // reusable transition canvas. Until it has a real frame, blending
            // from the outgoing snapshot to itself is visually neutral.
            second_offscreen_canvas->CopyFrom(*first_offscreen_canvas);
        }
        return keep_running;
    };

    auto next_continue = render_next();
    std::uint64_t local_frames = 0;
    tmillis_t first_local_frame_ms = 0;

    while (true) {
        const auto now_ms = time_source_->now_ms();
        if (!runtime_inputs_valid())
            return;
        if (now_ms - transition_start_ms > max_transition_ms) {
            apply_transition_frame(composite_offscreen_canvas, first_offscreen_canvas, second_offscreen_canvas, 1.0f, matrix_width,
                                   matrix_height, transition_name);
            forced_scene = next_scene;
            break;
        }

        const auto elapsed = now_ms - transition_start_ms;
        const auto alpha =
            std::clamp(static_cast<float>(elapsed) / static_cast<float>(std::max<tmillis_t>(1, transition_duration)), 0.0f, 1.0f);

        // Ease scene handoffs in and out. Linear alpha makes the first few
        // frames of high-contrast scenes read as a luminance pop on LEDs.
        const float eased_alpha = alpha * alpha * (3.0f - 2.0f * alpha);
        const auto current_visibility = 1.0f - eased_alpha;
        const auto next_visibility = eased_alpha;

        // The outgoing scene is intentionally a frozen snapshot. Rendering both
        // scenes throughout a transition doubles the worst-case Pi workload for
        // almost no perceptual benefit once the old scene is fading away.
        (void)current_visibility;

        if ((now_ms - last_next_render_ms) >= render_interval_ms_from_visibility(next_visibility)) {
            next_continue = render_next();
            last_next_render_ms = now_ms;
        }

        if (!current_continue || !next_continue || *interrupt_flag_ || *exit_flag_) {
            trace("Exiting scene early.");
            forced_scene = next_scene;
            break;
        }

        if (local_frames++ == 0) first_local_frame_ms = now_ms;
        apply_transition_frame(composite_offscreen_canvas, first_offscreen_canvas, second_offscreen_canvas, eased_alpha, matrix_width,
                               matrix_height, transition_name);

        if (post_processor_)
            post_processor_->apply_effects(composite_offscreen_canvas);

        LiveFrame::SnapshotStore::instance().capture_if_requested(composite_offscreen_canvas, matrix_width, matrix_height);

        composite_offscreen_canvas = matrix_->SwapOnVSync(composite_offscreen_canvas, 1);

        presenter_->present();
        Diagnostics::RuntimeDiagnostics::instance().record_presentation(60);

        if (alpha >= 1.0f) {
            const auto elapsed_ms = now_ms - first_local_frame_ms;
            Diagnostics::RuntimeDiagnostics::instance().set_transition_state({
                {"name", transition_name}, {"from", scene->get_name()}, {"to", next_scene->get_name()},
                {"placement", "local"}, {"frames", local_frames}, {"startup_ms", 0},
                {"presentation_fps", elapsed_ms > 0 && local_frames > 1
                    ? 1000.0 * static_cast<double>(local_frames - 1) / static_cast<double>(elapsed_ms) : 0.0}});
            forced_scene = next_scene;
            break;
        }
    }
}

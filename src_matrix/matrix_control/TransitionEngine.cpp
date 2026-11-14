#include "TransitionEngine.h"

#include <chrono>

#include "shared/common/crash_reporter.h"
#include "shared/matrix/diagnostics.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/transition_composer.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"

using namespace spdlog;

namespace {
/// The `set_transition_state` record every transition ends with. `details` carries the fields
/// that depend on where the transition was composed.
void record_transition(const std::string& name, const Scenes::Scene& from, const Scenes::Scene& to, const char* placement,
                       nlohmann::json details)
{
    nlohmann::json state{{"name", name}, {"from", from.get_name()}, {"to", to.get_name()}, {"placement", placement}};
    state.update(details);
    Diagnostics::RuntimeDiagnostics::instance().set_transition_state(std::move(state));
}
}  // namespace

TransitionEngine::TransitionEngine(TimeSource* time_source, TransitionManager* transition_manager, FrameSink* sink,
                                   const std::atomic<bool>* exit_flag, const std::atomic<bool>* interrupt_flag, DesktopHandoff* handoff)
    : time_source_(time_source),
      transition_manager_(transition_manager),
      sink_(sink),
      exit_flag_(exit_flag),
      interrupt_flag_(interrupt_flag),
      own_handoff_(handoff ? nullptr : std::make_unique<DesktopHandoff>()),
      handoff_(handoff ? handoff : own_handoff_.get())
{
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

TransitionResult TransitionEngine::render_transition_phase(std::shared_ptr<Scenes::Scene> scene, std::shared_ptr<Scenes::Scene> next_scene,
                                                           FrameCanvas* first_offscreen_canvas, FrameCanvas* second_offscreen_canvas,
                                                           FrameCanvas*& composite_offscreen_canvas, int matrix_width, int matrix_height,
                                                           tmillis_t transition_duration, const std::string& transition_name,
                                                           tmillis_t start_delay_ms, std::function<bool()> inputs_still_available)
{
    TransitionResult result;
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
        return false;
    };

    if (start_delay_ms > 0) {
        const auto hold_start = time_source_->now_ms();
        while (time_source_->now_ms() - hold_start < start_delay_ms) {
            if (*interrupt_flag_ || *exit_flag_ || !runtime_inputs_valid())
                return {};
            bool frame_updated = false;
            try {
                frame_updated = handoff_->copy_scene_frame(*scene, composite_offscreen_canvas, matrix_width, matrix_height);
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
            sink_->present(composite_offscreen_canvas, scene->get_declared_target_fps());
        }
    }

    Diagnostics::RuntimeDiagnostics::instance().mark_transition_composing();
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
        return {};

    // Snapshot the outgoing frame before changing any remote session. This is
    // especially important when the current scene is already desktop-rendered:
    // its local simulation may have intentionally stopped advancing.
    bool current_continue = true;
    bool current_frame_updated = false;
    current_frame_updated = handoff_->copy_scene_frame(*scene, first_offscreen_canvas, matrix_width, matrix_height);
    if (!current_frame_updated) {
        current_continue = safe_render(scene, first_offscreen_canvas);
        current_frame_updated = scene->frame_was_updated();
    }
    if (!current_frame_updated && sink_->visible() != nullptr) {
        // The outgoing scene has no new pixels. Snapshot the canvas the sink
        // most recently latched on the matrix instead of exposing arbitrary
        // contents left in this reusable transition buffer. This copy happens
        // only at the transition boundary, not every frame.
        first_offscreen_canvas->CopyFrom(*sink_->visible());
        current_frame_updated = true;
    }

    // The worker composes the transition when it can; the Pi only writes the finished pixels to GPIO.
    const auto request_started = std::chrono::steady_clock::now();
    const bool desktop_started = handoff_->begin_desktop_transition(*next_scene, *first_offscreen_canvas, matrix_width, matrix_height,
                                                                    transition_name, transition_duration, time_source_->now_ms());
    // Render-thread time spent snapshotting and sending the start command.
    const double request_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - request_started).count();
    if (desktop_started) {
        while (!*interrupt_flag_ && !*exit_flag_) {
            if (!runtime_inputs_valid())
                return {};
            const auto now = time_source_->now_ms();
            const auto tick = handoff_->desktop_transition_tick(composite_offscreen_canvas, now);
            if (tick.frame) {
                sink_->present(composite_offscreen_canvas, 60);
            } else {
                SleepMillis(1);
            }
            using Outcome = DesktopHandoff::TransitionOutcome;
            if (tick.outcome == Outcome::Done) {
                const auto stats = handoff_->desktop_transition_stats(now);
                record_transition(transition_name, *scene, *next_scene, "desktop",
                                  {{"frames", stats.frames},
                                   {"startup_ms", stats.startup_ms},
                                   {"request_ms", request_ms},
                                   {"presentation_fps", stats.presentation_fps}});
                result.forced_scene = next_scene;
                return result;  // The session continues as the scene's.
            }
            if (tick.outcome == Outcome::Abandoned) {
                const auto stats = handoff_->desktop_transition_stats(now);
                record_transition(transition_name, *scene, *next_scene, "desktop",
                                  {{"outcome", "abandoned"},
                                   {"frames", stats.frames},
                                   {"startup_ms", stats.startup_ms},
                                   {"request_ms", request_ms},
                                   {"presentation_fps", stats.presentation_fps}});
                result.forced_scene = next_scene;
                return result;
            }
            if (tick.outcome == Outcome::Failed)
                break; // Worker never produced a frame: compose locally.
        }
        if (*interrupt_flag_ || *exit_flag_)
            return {};
    }

    handoff_->begin_local_transition(*next_scene, matrix_width, matrix_height);

    // The Pi freezes the outgoing scene (the snapshot in `first_offscreen_canvas`); see OutgoingPolicy.
    // The blend clock started before the desktop attempt, so a stalled worker eats into it.
    TransitionComposer composer(transition_manager_, TransitionComposer::OutgoingPolicy::Freeze);
    composer.begin(first_offscreen_canvas, transition_name, transition_duration, matrix_width, matrix_height, transition_start_ms);

    // Whether the incoming canvas received a real frame since the composer last saw it.
    bool incoming_updated = false;
    const auto render_next = [&]() {
        if (handoff_->incoming_frame(second_offscreen_canvas)) {
            incoming_updated = true;
            return true;
        }

        const bool keep_running = safe_render(next_scene, second_offscreen_canvas);
        incoming_updated = incoming_updated || next_scene->frame_was_updated();
        return keep_running;
    };

    auto next_continue = render_next();
    std::uint64_t local_frames = 0;
    tmillis_t first_local_frame_ms = 0;

    while (true) {
        const auto now_ms = time_source_->now_ms();
        if (!runtime_inputs_valid())
            return {};
        if (now_ms - transition_start_ms > max_transition_ms) {
            result.forced_scene = next_scene;
            break;
        }

        const float next_visibility = composer.eased_progress(now_ms);
        if ((now_ms - last_next_render_ms) >= render_interval_ms_from_visibility(next_visibility)) {
            next_continue = render_next();
            last_next_render_ms = now_ms;
        }

        if (!current_continue || !next_continue || *interrupt_flag_ || *exit_flag_) {
            trace("Exiting scene early.");
            result.forced_scene = next_scene;
            break;
        }

        if (local_frames++ == 0) first_local_frame_ms = now_ms;
        const auto frame = composer.frame(composite_offscreen_canvas, second_offscreen_canvas, incoming_updated, now_ms);
        incoming_updated = false;

        sink_->present(composite_offscreen_canvas, 60);

        if (frame.done) {
            const auto elapsed_ms = now_ms - first_local_frame_ms;
            record_transition(transition_name, *scene, *next_scene, "local",
                              {{"frames", local_frames},
                               {"startup_ms", 0},
                               // The desktop was asked first but sent no frame within the startup timeout.
                               {"desktop_fallback", desktop_started},
                               {"presentation_fps", elapsed_ms > 0 && local_frames > 1
                                                        ? 1000.0 * static_cast<double>(local_frames - 1) / static_cast<double>(elapsed_ms)
                                                        : 0.0}});
            result.forced_scene = next_scene;
            break;
        }
    }
    return result;
}

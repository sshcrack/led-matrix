#include "DesktopHandoff.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "led-matrix.h"
#include "shared/common/crash_reporter.h"
#include "shared/common/remote_render_protocol.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/diagnostics.h"

namespace {
constexpr tmillis_t MaxTransitionMs = 10000;

class RemoteRenderLink final : public DesktopLink {
public:
    bool worker_available(std::string_view scene) override { return RemoteRender::worker_available(scene); }
    std::optional<std::uint32_t> request_scene(const Scenes::Scene &scene, int width, int height, int target_fps,
                                               const nlohmann::json &transition) override
    {
        return RemoteRender::request_scene(scene, width, height, target_fps, transition);
    }
    void publish_runtime_state(std::uint32_t session) override { RemoteRender::publish_runtime_state(session); }
    bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas, int width, int height,
                     double max_age_ms) override
    {
        return RemoteRender::copy_latest(session, canvas, width, height, max_age_ms);
    }
    std::optional<double> latest_frame_age_ms(std::uint32_t session) override
    {
        return RemoteRender::latest_frame_age_ms(session);
    }
    RemoteRender::Status status() override { return RemoteRender::status(); }
    void stop() override { RemoteRender::stop(); }
    std::uint32_t latest_sequence(std::uint32_t session) override { return RemoteRender::latest_sequence(session); }
    std::uint32_t copy_latest_sequence(std::uint32_t session, rgb_matrix::FrameCanvas* canvas, int width, int height,
                                       double max_age_ms) override
    {
        std::uint32_t sequence = 0;
        return RemoteRender::copy_latest(session, canvas, width, height, max_age_ms, &sequence) ? sequence : 0;
    }
    bool accepts_base64_snapshot() override
    {
        return RemoteRender::worker_has_feature(RemoteRenderProtocol::Features::TransitionSnapshotBase64);
    }
    bool accepts_frame_reference() override
    {
        return RemoteRender::worker_has_feature(RemoteRenderProtocol::Features::TransitionFromFrame);
    }
};

double frame_budget_ms(const Scenes::Scene &scene)
{
    return 1000.0 / static_cast<double>(std::max(1, scene.get_declared_target_fps()));
}
} // namespace

std::uint32_t DesktopLink::latest_sequence(std::uint32_t session)
{
    const auto remote = status();
    return remote.session == session ? remote.last_sequence : 0;
}

std::uint32_t DesktopLink::copy_latest_sequence(std::uint32_t session, rgb_matrix::FrameCanvas* canvas, int width, int height,
                                                double max_age_ms)
{
    const auto sequence = latest_sequence(session);
    return copy_latest(session, canvas, width, height, max_age_ms) ? sequence : 0;
}

DesktopLink &DesktopLink::remote_render()
{
    static RemoteRenderLink link;
    return link;
}

std::optional<std::uint32_t> DesktopHandoff::request(const Scenes::Scene &scene, int width, int height, int fps,
                                                     const nlohmann::json &transition)
{
    auto session = link_.request_scene(scene, width, height, fps, transition);
    if (session) {
        session_ = session;
        session_scene_ = scene.get_name();
    }
    return session;
}

void DesktopHandoff::stop()
{
    link_.stop();
    session_.reset();
    session_scene_.clear();
}

std::optional<std::string> DesktopHandoff::offload_reason(const Scenes::Scene &scene)
{
    // The desktop always renders full quality, so a quality-reduced Pi scene goes there too.
    if (scene.get_render_quality_scale() < ReducedQualityThreshold)
        return "Pi renders this scene at reduced quality; the desktop renders it at full quality";
    const auto p95 = Diagnostics::RuntimeDiagnostics::instance().scene_render_p95(scene.get_name());
    if (p95.has_value() && *p95 > frame_budget_ms(scene) * RenderPlacement::PressureBudgetFraction)
        return "measured local p95 exceeds 72% of the frame budget";
    return std::nullopt;
}

// ---- scene phase ----

void DesktopHandoff::request_scene_desktop()
{
    placement_.request_desktop(request(*scene_, width_, height_, scene_->get_declared_target_fps()));
}

void DesktopHandoff::report(const std::string &reason, nlohmann::json extra)
{
    const auto mode = placement_.mode();
    const char *activity = mode == RenderPlacement::Mode::DesktopLive ? "desktop"
        : mode == RenderPlacement::Mode::DesktopPending ? "desktop pending"
        : mode == RenderPlacement::Mode::LocalFallback ? "local fallback" : "local";
    CrashReporter::set_activity(std::string("rendering scene '") + scene_->get_name() + "' (" + activity + ")");
    nlohmann::json payload{
        {"scene", scene_->get_name()},
        {"placement", RenderPlacement::label(mode)},
        {"reason", reason},
        {"frame_budget_ms", budget_ms_},
        {"quality_scale", mode == RenderPlacement::Mode::DesktopLive ? 1.0f : scene_->get_render_quality_scale()},
    };
    payload.update(extra);
    Diagnostics::RuntimeDiagnostics::instance().set_render_placement(std::move(payload));
}

void DesktopHandoff::begin_scene(const Scenes::Scene &scene, int width, int height)
{
    scene_ = &scene;
    width_ = width;
    height_ = height;
    budget_ms_ = frame_budget_ms(scene);
    placement_ = RenderPlacement{};
    transition_.reset();
    incoming_.reset();

    const auto caps = scene.get_capabilities();
    reason_ = "local render is within the measured frame budget";
    if (caps.supports_remote_rendering && link_.worker_available(scene.get_name())) {
        if (session_ && session_scene_ == scene.get_name()) {
            // A transition warmed this scene on the desktop. Re-requesting reuses
            // the session while the scene's properties are unchanged.
            request_scene_desktop();
            reason_ = "desktop session was warmed during the transition";
        } else if (auto why = offload_reason(scene)) {
            request_scene_desktop();
            reason_ = std::move(*why);
        }
    } else {
        const auto remote = link_.status();
        if (session_ || remote.requested)
            stop();
        if (!caps.supports_remote_rendering)
            reason_ = "scene explicitly opted out of generic desktop execution; adaptive local quality is active";
        else if (remote.worker_available && !remote.worker_compatible)
            reason_ = "desktop scene worker runs different scene code (update the desktop app to this version); rendering locally";
        else
            reason_ = "desktop scene worker is unavailable; rendering locally";
    }
    report(reason_);
}

DesktopHandoff::FrameSource DesktopHandoff::scene_frame(rgb_matrix::FrameCanvas *canvas)
{
    if (placement_.has_session() && !link_.worker_available(scene_->get_name())) {
        placement_.worker_lost();
        report("desktop scene worker became unavailable; immediate local fallback");
    }
    if (const auto session = placement_.session()) {
        link_.publish_runtime_state(*session);
        if (link_.copy_latest(*session, canvas, width_, height_, SceneFrameMaxAgeMs)) {
            if (placement_.remote_frame_presented())
                report(reason_);
            return FrameSource::Remote;
        }
    }
    if (placement_.mode() == RenderPlacement::Mode::DesktopLive) {
        const auto age = link_.latest_frame_age_ms(*placement_.session());
        if (placement_.no_remote_frame(age) == RenderPlacement::NoRemoteFrame::HoldRemote)
            return FrameSource::HoldRemote;
        report("remote frame became stale; continuing locally without blocking");
    }
    return FrameSource::Local;
}

void DesktopHandoff::local_frame_rendered(double active_render_ms)
{
    // Learn on the actual Pi instead of guessing from x86/QEMU timings. Sustained
    // pressure starts the desktop in parallel; local rendering continues until the
    // first complete remote frame arrives, so there is never a blank handoff.
    if (!placement_.local_frame_rendered(active_render_ms, budget_ms_)
        || !scene_->get_capabilities().supports_remote_rendering
        || !link_.worker_available(scene_->get_name()))
        return;
    request_scene_desktop();
    if (placement_.has_session()) {
        reason_ = "live Pi render cost exceeded 72% of the frame budget for four frames";
        report(reason_, {{"local_render_ms", active_render_ms}});
    }
}

// ---- transition phase ----

bool DesktopHandoff::copy_scene_frame(const Scenes::Scene &scene, rgb_matrix::FrameCanvas *canvas, int width, int height)
{
    copied_frame_.reset();
    if (!session_ || session_scene_ != scene.get_name())
        return false;
    link_.publish_runtime_state(*session_);
    const auto sequence = link_.copy_latest_sequence(*session_, canvas, width, height, SceneFrameMaxAgeMs);
    if (sequence == 0)
        return false;
    copied_frame_.emplace(*session_, sequence);
    return true;
}

bool DesktopHandoff::begin_desktop_transition(const Scenes::Scene &next, const rgb_matrix::FrameCanvas &outgoing,
                                              int width, int height, const std::string &name, tmillis_t duration_ms,
                                              tmillis_t now_ms)
{
    transition_.reset();
    const auto copied = std::exchange(copied_frame_, std::nullopt);
    if (!next.get_capabilities().supports_remote_rendering || !link_.worker_available(next.get_name()))
        return false;
    const auto duration = std::clamp<tmillis_t>(duration_ms, 1, MaxTransitionMs);
    nlohmann::json transition{{"name", name}, {"duration_ms", duration}};
    if (copied && link_.accepts_frame_reference()) {
        // The outgoing frame came from the worker: point it at that frame
        // instead of reading back, encoding and resending ~49 KB.
        transition["from_frame"] = {{"session", copied->first}, {"sequence", copied->second}};
    }
    else {
        // One outgoing RGB snapshot; the worker owns transition timing and composition.
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 3));
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto offset = static_cast<std::size_t>((y * width + x) * 3);
                outgoing.GetPixel(x, y, &pixels[offset], &pixels[offset + 1], &pixels[offset + 2]);
            }
        // A JSON array of 49k numbers took the Pi's render thread ~40 ms to build,
        // copy and serialize while the outgoing frame stood still.
        if (link_.accepts_base64_snapshot())
            transition["from_base64"] = RemoteRenderProtocol::encode_base64(pixels);
        else
            transition["from"] = std::move(pixels);
    }
    if (!request(next, width, height, 60, transition))
        return false;
    transition_ = Transition{.next = next.get_name(), .duration_ms = duration, .requested_at = now_ms,
                             .last_frame_at = now_ms, .width = width, .height = height};
    return true;
}

DesktopHandoff::TransitionTick DesktopHandoff::desktop_transition_tick(rgb_matrix::FrameCanvas *canvas, tmillis_t now_ms)
{
    auto &t = *transition_;
    TransitionTick tick;
    link_.publish_runtime_state(*session_);
    // Lock-free: status() locks the transport and scans the contract maps.
    std::uint32_t sequence = 0;
    if (link_.latest_sequence(*session_) != t.last_sequence &&
        (sequence = link_.copy_latest_sequence(*session_, canvas, t.width, t.height, TransitionFrameMaxAgeMs)) != 0 &&
        sequence != t.last_sequence) {
        t.last_sequence = sequence;
        t.last_frame_at = now_ms;
        if (t.frames++ == 0) t.first_frame_at = now_ms;
        tick.frame = true;
    }
    const bool presented = t.frames > 0;
    // The worker starts its blend clock when the start command arrives, and its
    // first frame arrives here shortly after. Count the duration from that frame,
    // not from the request, so the Pi does not leave the transition mid-blend
    // (and change the target FPS) when the start took 100+ ms.
    if (presented && now_ms - t.first_frame_at >= t.duration_ms) {
        tick.outcome = TransitionOutcome::Done;   // the session continues as the scene's
    }
    else if (now_ms - t.last_frame_at > (presented ? TransitionStallMs : TransitionStartupMs)) {
        stop();
        // Do not replay a half-finished handoff after a stall; one never started is composed locally.
        tick.outcome = presented ? TransitionOutcome::Abandoned : TransitionOutcome::Failed;
    }
    return tick;
}

DesktopHandoff::TransitionStats DesktopHandoff::desktop_transition_stats(tmillis_t now_ms) const
{
    if (!transition_) return {};
    const auto &t = *transition_;
    const auto elapsed_ms = now_ms - t.first_frame_at;
    return {.frames = t.frames,
            .startup_ms = t.first_frame_at - t.requested_at,
            .presentation_fps = elapsed_ms > 0 && t.frames > 1
                ? 1000.0 * static_cast<double>(t.frames - 1) / static_cast<double>(elapsed_ms) : 0.0};
}

void DesktopHandoff::begin_local_transition(const Scenes::Scene &next, int width, int height)
{
    incoming_.reset();
    incoming_width_ = width;
    incoming_height_ = height;
    if (next.get_capabilities().supports_remote_rendering && link_.worker_available(next.get_name())
        && offload_reason(next).has_value())
        incoming_ = request(next, width, height, next.get_declared_target_fps());
    if (!incoming_ && (session_ || link_.status().requested))
        stop();
}

bool DesktopHandoff::incoming_frame(rgb_matrix::FrameCanvas *canvas)
{
    if (!incoming_) return false;
    link_.publish_runtime_state(*incoming_);
    return link_.copy_latest(*incoming_, canvas, incoming_width_, incoming_height_, SceneFrameMaxAgeMs);
}

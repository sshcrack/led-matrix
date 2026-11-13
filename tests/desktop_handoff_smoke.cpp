// Desktop handoff through its one seam: an in-memory desktop worker behind
// DesktopLink and a controlled clock (the `now_ms` / frame-age arguments).
#include <emulator.h>
#include <led-matrix.h>
#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/diagnostics.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "matrix_control/DesktopHandoff.h"

namespace {
int failures = 0;
void expect(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

class ProbeScene final : public Scenes::Scene {
public:
    explicit ProbeScene(std::string name) : name_(std::move(name)) {}
    bool render(rgb_matrix::FrameCanvas *canvas) override { canvas->Fill(1, 2, 3); return true; }
    void register_properties() override {}
    std::string get_name() const override { return name_; }

protected:
    tmillis_t get_default_duration() override { return 2000; }
    int get_default_weight() override { return 1; }

private:
    std::string name_;
};

/// One session at a time, like RemoteRender. Frames are "latest-wins": a copy
/// succeeds while the newest frame is younger than the caller's limit.
class FakeWorker final : public DesktopLink {
public:
    bool available = true;
    std::uint32_t session = 0;
    std::string scene;
    std::uint32_t sequence = 0;
    std::optional<double> frame_age;
    std::vector<nlohmann::json> starts;   // transition payload of every new session
    int stops = 0;
    bool base64 = false;

    void frame(double age = 0.0) { ++sequence; frame_age = age; }
    bool accepts_base64_snapshot() override { return base64; }

    bool worker_available(std::string_view) override { return available; }
    std::optional<std::uint32_t> request_scene(const Scenes::Scene &s, int, int, int,
                                               const nlohmann::json &transition) override
    {
        if (!available) return std::nullopt;
        if (transition.empty() && session != 0 && scene == s.get_name()) return session;   // reuse
        session = next_++;
        scene = s.get_name();
        sequence = 0;
        frame_age.reset();
        starts.push_back(transition);
        return session;
    }
    void publish_runtime_state(std::uint32_t) override {}
    bool copy_latest(std::uint32_t s, rgb_matrix::FrameCanvas *canvas, int, int, double max_age_ms) override
    {
        if (s != session || !frame_age || *frame_age > max_age_ms) return false;
        canvas->Fill(200, 100, 50);
        return true;
    }
    std::optional<double> latest_frame_age_ms(std::uint32_t s) override
    {
        return s == session ? frame_age : std::nullopt;
    }
    RemoteRender::Status status() override
    {
        RemoteRender::Status st;
        st.requested = session != 0;
        st.session = session;
        st.last_sequence = sequence;
        st.scene = scene;
        st.worker_available = available;
        st.worker_compatible = true;
        return st;
    }
    void stop() override { ++stops; session = 0; scene.clear(); frame_age.reset(); }

private:
    std::uint32_t next_ = 1;
};

std::shared_ptr<ProbeScene> make_scene(const std::string &name)
{
    auto scene = std::make_shared<ProbeScene>(name);
    scene->update_default_properties();
    scene->register_properties();
    scene->load_properties(nlohmann::json::object());
    scene->initialize(32, 32);
    return scene;
}

using Mode = RenderPlacement::Mode;
using Source = DesktopHandoff::FrameSource;
using Outcome = DesktopHandoff::TransitionOutcome;
} // namespace

int main()
{
    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) { std::cerr << "failed to create emulator matrix\n"; return 1; }
    auto *outgoing = matrix->CreateFrameCanvas();
    auto *canvas = matrix->CreateFrameCanvas();
    const auto from = make_scene("handoff-from");
    const auto to = make_scene("handoff-to");

    {   // Desktop transition → the warmed session carries into the scene → stall → recovery → worker loss.
        FakeWorker worker;
        DesktopHandoff handoff(worker);
        outgoing->Fill(9, 9, 9);
        expect(handoff.begin_desktop_transition(*to, *outgoing, 32, 32, "blend", 400, 0), "desktop transition starts");
        expect(worker.starts.size() == 1 && worker.starts[0].at("from").size() == 32 * 32 * 3
                   && worker.starts[0].at("duration_ms") == 400,
               "the worker gets the outgoing pixels and duration");
        auto tick = handoff.desktop_transition_tick(canvas, 100);
        expect(!tick.frame && tick.outcome == Outcome::Continue, "no frame yet: keep waiting within 500 ms");
        worker.frame();
        tick = handoff.desktop_transition_tick(canvas, 200);
        expect(tick.frame && tick.outcome == Outcome::Continue, "first composed frame is presented");
        tick = handoff.desktop_transition_tick(canvas, 250);
        expect(!tick.frame, "the same frame is not presented twice");
        worker.frame();
        tick = handoff.desktop_transition_tick(canvas, 400);
        expect(tick.frame && tick.outcome == Outcome::Done, "transition finishes after its duration");
        const auto stats = handoff.desktop_transition_stats(400);
        expect(stats.frames == 2 && stats.startup_ms == 200 && stats.presentation_fps == 5.0, "transition stats");

        handoff.begin_scene(*to, 32, 32);
        expect(worker.starts.size() == 1 && worker.stops == 0, "the warmed session is adopted, not restarted");
        expect(handoff.scene_mode() == Mode::DesktopPending, "adopted session awaits its first scene frame");
        worker.frame_age = 10.0;
        expect(handoff.scene_frame(canvas) == Source::Remote && handoff.scene_mode() == Mode::DesktopLive,
               "fresh frame goes live");
        worker.frame_age = 800.0;
        expect(handoff.scene_frame(canvas) == Source::HoldRemote, "a stall up to 1500 ms holds the desktop frame");
        worker.frame_age = 1600.0;
        expect(handoff.scene_frame(canvas) == Source::Local && handoff.scene_mode() == Mode::LocalFallback,
               "a longer stall renders locally");
        worker.frame_age = 5.0;
        expect(handoff.scene_frame(canvas) == Source::Remote && handoff.scene_mode() == Mode::DesktopLive,
               "fresh desktop frames resume");
        worker.available = false;
        expect(handoff.scene_frame(canvas) == Source::Local && handoff.scene_mode() == Mode::Local,
               "worker loss falls back immediately");
    }

    {  // A worker that advertises it gets the compact snapshot with the same pixels.
        FakeWorker array_worker, base64_worker;
        base64_worker.base64 = true;
        DesktopHandoff array_handoff(array_worker), base64_handoff(base64_worker);
        outgoing->Fill(9, 120, 250);
        expect(array_handoff.begin_desktop_transition(*to, *outgoing, 32, 32, "blend", 400, 0) &&
                   base64_handoff.begin_desktop_transition(*to, *outgoing, 32, 32, "blend", 400, 0),
               "both encodings start a transition");
        const auto& array_start = array_worker.starts.at(0);
        const auto& base64_start = base64_worker.starts.at(0);
        const auto decoded = RemoteRenderProtocol::decode_base64(base64_start.at("from_base64").get<std::string>());
        expect(!base64_start.contains("from") && !array_start.contains("from_base64") && decoded &&
                   *decoded == array_start.at("from").get<std::vector<std::uint8_t>>(),
               "the base64 snapshot carries exactly the array's pixels");
    }

    {   // The worker never answers a transition: compose locally; nothing lingers.
        FakeWorker worker;
        DesktopHandoff handoff(worker);
        expect(handoff.begin_desktop_transition(*to, *outgoing, 32, 32, "blend", 400, 0), "transition requested");
        expect(handoff.desktop_transition_tick(canvas, 500).outcome == Outcome::Continue, "500 ms startup grace");
        expect(handoff.desktop_transition_tick(canvas, 501).outcome == Outcome::Failed && worker.stops == 1,
               "no first frame within 500 ms fails over to local");
        handoff.begin_local_transition(*to, 32, 32);
        expect(!handoff.incoming_frame(canvas) && worker.session == 0, "a cheap incoming scene renders locally");
    }

    {   // The worker stalls mid-transition: skip to the scene, do not replay the half-finished handoff.
        FakeWorker worker;
        DesktopHandoff handoff(worker);
        expect(handoff.begin_desktop_transition(*to, *outgoing, 32, 32, "blend", 1000, 0), "transition requested");
        worker.frame();
        expect(handoff.desktop_transition_tick(canvas, 100).frame, "first frame");
        expect(handoff.desktop_transition_tick(canvas, 250).outcome == Outcome::Continue, "150 ms gap tolerated");
        expect(handoff.desktop_transition_tick(canvas, 251).outcome == Outcome::Abandoned && worker.stops == 1,
               "a longer gap abandons the transition");
        handoff.begin_scene(*to, 32, 32);
        expect(worker.starts.size() == 1 && handoff.scene_mode() == Mode::Local,
               "the abandoned session is not adopted by the scene");
    }

    {   // Outgoing desktop frame; quality-reduced incoming scene is offloaded for a local transition.
        FakeWorker worker;
        DesktopHandoff handoff(worker);
        from->force_render_quality_scale(0.6f);
        handoff.begin_scene(*from, 32, 32);
        expect(handoff.scene_mode() == Mode::DesktopPending && worker.scene == from->get_name(),
               "a quality-reduced scene starts on the desktop");
        worker.frame_age = 0.0;
        expect(handoff.copy_scene_frame(*from, outgoing, 32, 32), "outgoing desktop frame is copied");
        expect(!handoff.copy_scene_frame(*to, outgoing, 32, 32), "another scene's session is not copied");
        to->force_render_quality_scale(0.6f);
        handoff.begin_local_transition(*to, 32, 32);
        expect(worker.scene == to->get_name() && worker.stops == 0, "the incoming scene is warmed on the desktop");
        worker.frame_age = 0.0;
        expect(handoff.incoming_frame(canvas), "incoming desktop frames feed the local composition");
        handoff.begin_scene(*to, 32, 32);
        expect(worker.starts.size() == 2 && handoff.scene_mode() == Mode::DesktopPending,
               "the scene adopts the session warmed by the local transition");
        from->force_render_quality_scale(1.0f);
        to->force_render_quality_scale(1.0f);
    }

    {   // Sustained local pressure starts the desktop; an unusable worker drops a lingering session.
        FakeWorker worker;
        DesktopHandoff handoff(worker);
        handoff.begin_scene(*from, 32, 32);
        expect(handoff.scene_mode() == Mode::Local, "within budget: local");
        const double over = 1000.0 / from->get_declared_target_fps() * 0.9;
        for (unsigned i = 0; i < RenderPlacement::PressureFramesToOffload; ++i)
            handoff.local_frame_rendered(over);
        expect(handoff.scene_mode() == Mode::DesktopPending && worker.session != 0,
               "four over-budget frames start the desktop");
        worker.available = false;
        handoff.begin_scene(*to, 32, 32);
        expect(worker.stops == 1 && worker.session == 0 && handoff.scene_mode() == Mode::Local,
               "no usable worker: the session is stopped");
    }

    {  // Transition timeline: outgoing freeze, transition frames and the handoff gap.
        auto& diagnostics = Diagnostics::RuntimeDiagnostics::instance();
        const auto history_size = [&] { return diagnostics.snapshot()["transition_history"].size(); };
        const auto pause = [] { std::this_thread::sleep_for(std::chrono::milliseconds(3)); };
        const auto before = history_size();
        diagnostics.record_presentation(60);  // last outgoing frame
        pause();
        diagnostics.begin_transition_timeline();
        diagnostics.record_presentation(60);  // beat-synchronised hold: still outgoing
        diagnostics.mark_transition_composing();
        pause();
        for (int i = 0; i < 3; ++i) {
            diagnostics.record_presentation(60);
            pause();
        }
        diagnostics.end_transition_timeline();
        expect(history_size() == before, "the record waits for the handoff gap");
        diagnostics.record_presentation(60);  // first scene-phase frame
        const auto history = diagnostics.snapshot()["transition_history"];
        expect(history.size() == before + 1, "a finished transition is recorded");
        const auto& record = history.back();
        expect(record.value("presented_frames", 0) == 3, "only composed frames count as transition frames");
        expect(record.at("lead_in_gap_ms").is_number() && record.at("lead_in_gap_ms").get<double>() >= 3.0 &&
                   record.at("lead_in_gap_ms").get<double>() < record.at("setup_ms").get<double>() + 50.0,
               "the freeze is measured from the last outgoing frame, including the hold");
        expect(record.at("handoff_gap_ms").is_number() && record.at("interval_ms_max").is_number(),
               "handoff gap and intervals are reported");

        diagnostics.begin_transition_timeline();
        diagnostics.cancel_transition_timeline();
        diagnostics.record_presentation(60);
        diagnostics.begin_transition_timeline();
        diagnostics.end_transition_timeline();  // aborted before composing
        diagnostics.record_presentation(60);
        expect(history_size() == before + 1, "cancelled and aborted transitions add no record");
    }

    if (failures == 0) std::cout << "DesktopHandoff OK\n";
    return failures == 0 ? 0 : 1;
}

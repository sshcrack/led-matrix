// Desktop handoff through its one seam: an in-memory desktop worker behind
// DesktopLink and a controlled clock (the `now_ms` / frame-age arguments).
#include "matrix_control/DesktopHandoff.h"

#include <shared/matrix/Scene.h>

#include <emulator.h>
#include <led-matrix.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

    void frame(double age = 0.0) { ++sequence; frame_age = age; }

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

    if (failures == 0) std::cout << "DesktopHandoff OK\n";
    return failures == 0 ? 0 : 1;
}

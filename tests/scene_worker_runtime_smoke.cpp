// The desktop scene worker's RenderRuntime, run in-process (no worker executable, no plugins):
// session start/stop, the blend into a session's first frames, a transition that starts from a
// frame the worker sent itself, and the outgoing scene that keeps moving under that blend.
#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/wrappers.h>

#include <array>
#include <iostream>
#include <memory>
#include <string>

#include "RenderRuntime.h"

namespace {

using SceneWorker::ManualClock;
using SceneWorker::RenderRuntime;

int failures = 0;

void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
        ++failures;
    }
}

/// Paints a different grey every frame, so "keeps moving" is observable.
class CountingScene : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas* canvas) override
    {
        const auto value = static_cast<std::uint8_t>(100 + (++frames % 100));
        canvas->Fill(value, value, value);
        return true;
    }
    void register_properties() override {}
    std::string get_name() const override { return "runtime_probe"; }
    Scenes::SceneCapabilities get_capabilities() const override
    {
        auto caps = Scene::get_capabilities();
        caps.supports_remote_rendering = true;
        return caps;
    }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
    int frames = 0;
};

/// Has nothing to show yet, like a media scene waiting for its artwork.
class HeldScene final : public CountingScene {
public:
    bool render(rgb_matrix::FrameCanvas*) override
    {
        hold_current_frame();
        return true;
    }
    std::string get_name() const override { return "runtime_probe_held"; }
};

template <class SceneType>
class ProbeWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<SceneType>(); }
};

/// out = from + (to - from) * alpha, so the blend is checkable in numbers.
class LinearBlend final : public TransitionEffect {
public:
    std::string get_name() const override { return "blend"; }
    void apply(FrameCanvas* dst, FrameCanvas* from, FrameCanvas* to, float alpha, int width, int height) override
    {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                std::uint8_t fr = 0, fg = 0, fb = 0, tr = 0, tg = 0, tb = 0;
                from->GetPixel(x, y, &fr, &fg, &fb);
                to->GetPixel(x, y, &tr, &tg, &tb);
                const auto mix = [alpha](std::uint8_t a, std::uint8_t b) {
                    return static_cast<std::uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * alpha +
                                                     0.5f);
                };
                dst->SetPixel(x, y, mix(fr, tr), mix(fg, tg), mix(fb, tb));
            }
    }
};

int red(rgb_matrix::FrameCanvas* canvas)
{
    std::uint8_t r = 0, g = 0, b = 0;
    canvas->GetPixel(3, 4, &r, &g, &b);
    return r;
}

nlohmann::json start_command(std::uint32_t session, const std::string& scene, nlohmann::json transition = nullptr)
{
    nlohmann::json command{{"protocol", RemoteRenderProtocol::Version},
                           {"session", session},
                           {"width", 32},
                           {"height", 32},
                           {"scene", scene},
                           {"scene_contract", 1}};
    if (!transition.is_null())
        command["transition"] = std::move(transition);
    return command;
}

nlohmann::json snapshot_transition(std::uint8_t value, int duration_ms, bool base64)
{
    const std::vector<std::uint8_t> rgb(32 * 32 * 3, value);
    nlohmann::json transition{{"name", "blend"}, {"duration_ms", duration_ms}};
    if (base64)
        transition["from_base64"] = RemoteRenderProtocol::encode_base64(rgb);
    else
        transition["from"] = rgb;
    return transition;
}

bool start_throws(RenderRuntime& runtime, const nlohmann::json& command)
{
    try {
        runtime.start(command);
    }
    catch (const std::exception&) {
        runtime.reset();
        return true;
    }
    return false;
}

}  // namespace

int main()
{
    auto* registry = Plugins::PluginManager::instance();
    (void)registry->get_scenes();
    registry->add_scene(std::make_shared<ProbeWrapper<CountingScene>>());
    registry->add_scene(std::make_shared<ProbeWrapper<HeldScene>>());

    TransitionManager transitions;
    transitions.register_transition(std::make_unique<LinearBlend>());

    // A session without a transition shows its scene as is.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe"));
        check(runtime.session == 1 && runtime.scene && runtime.matrix, "start creates the session");
        check(runtime.scene->render_frame(runtime.canvas, std::nullopt, true), "the scene renders");
        check(runtime.output_frame() == runtime.canvas && !runtime.composer.active(), "no transition: the scene's canvas is the output");

        const auto* original = runtime.scene.get();
        runtime.sequence = 9;
        runtime.apply_state({{"target_fps", 30}});
        check(runtime.target_fps == 30 && runtime.scene.get() == original && runtime.session == 1 && runtime.sequence == 9,
              "a rate change keeps the warmed scene");
        runtime.apply_state(nlohmann::json::object());
        check(runtime.target_fps == 30, "a legacy state message leaves the rate alone");

        runtime.reset();
        check(!runtime.scene && !runtime.matrix && !runtime.canvas && runtime.session == 0 && !runtime.composer.active(),
              "reset ends the session");
        check(start_throws(runtime, start_command(0, "runtime_probe")), "session 0 is rejected");
        auto stale = start_command(1, "runtime_probe");
        stale["protocol"] = RemoteRenderProtocol::Version + 1;
        check(start_throws(runtime, stale), "another protocol version is rejected");
    }

    // The blend into a session, with both snapshot encodings.
    for (const bool base64 : {true, false}) {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe", snapshot_transition(40, 1000, base64)));
        check(red(runtime.transition_from) == 40, std::string("the snapshot is decoded (") + (base64 ? "base64" : "array") + ")");
        check(runtime.composer.active(), "a transition starts a blend");

        runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
        const int incoming = red(runtime.canvas);
        check(runtime.output_frame() == runtime.transition_output && red(runtime.transition_output) == 40,
              "the blend starts on the snapshot");
        clock.advance_ms(500);
        check(runtime.output_frame() == runtime.transition_output, "mid-blend the output is the composed frame");
        check(std::abs(red(runtime.transition_output) - (40 + incoming) / 2) <= 1, "smoothstep puts the midpoint at half");
        clock.advance_ms(500);
        check(runtime.output_frame() == runtime.canvas && !runtime.composer.active(), "the finished blend hands over to the scene");
        check(runtime.output_frame() == runtime.canvas, "and stays handed over");
    }

    // The duration is clamped to the supported range.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe", snapshot_transition(40, 1'000'000, true)));
        runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
        clock.advance_ms(10000);
        check(runtime.output_frame() == runtime.canvas, "an absurd duration is capped at 10 s");
    }

    // An incoming scene without a frame yet blends from the snapshot instead of exposing a blank canvas.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe_held", snapshot_transition(40, 1000, true)));
        runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
        clock.advance_ms(500);
        check(runtime.output_frame() == runtime.transition_output && red(runtime.transition_output) == 40,
              "an unready incoming scene shows the outgoing snapshot");
        clock.advance_ms(600);
        check(runtime.output_frame() == runtime.canvas && red(runtime.canvas) == 40, "the held snapshot carries over the end of the blend");
        check(runtime.composer.active(), "the blend stays pending until the scene paints");
    }

    // Malformed or unsupported transitions fail the start and leave nothing behind.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        auto bad_size = start_command(1, "runtime_probe", snapshot_transition(40, 1000, true));
        bad_size["transition"]["from_base64"] = RemoteRenderProtocol::encode_base64(std::vector<std::uint8_t>(12, 1));
        check(start_throws(runtime, bad_size) && !runtime.scene, "a snapshot of the wrong size is rejected");
        auto bad_base64 = start_command(1, "runtime_probe", snapshot_transition(40, 1000, true));
        bad_base64["transition"]["from_base64"] = "not base64 !!";
        check(start_throws(runtime, bad_base64), "an undecodable snapshot is rejected");

        TransitionManager none;
        RenderRuntime without_effects(none, &clock);
        check(start_throws(without_effects, start_command(1, "runtime_probe", snapshot_transition(40, 1000, true))),
              "a worker without transition effects rejects the transition");
        check(without_effects.scene == nullptr, "and keeps no session");
    }

    // A transition can start from a frame the worker sent itself; the outgoing scene keeps moving under it.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe"));
        for (const std::uint8_t value : {std::uint8_t{40}, std::uint8_t{41}, std::uint8_t{42}}) {
            ++runtime.sequence;
            runtime.remember_sent_frame(std::vector<std::uint8_t>(32 * 32 * 3, value));
        }
        auto second = start_command(2, "runtime_probe");
        second["transition"] = {{"name", "blend"}, {"duration_ms", 750}, {"from_frame", {{"session", 1}, {"sequence", 2}}}};
        runtime.start(second);
        check(runtime.session == 2 && red(runtime.transition_from) == 41 && runtime.sent_frames.empty(),
              "the blend starts from the referenced frame");
        check(runtime.outgoing_scene && runtime.outgoing_scene->get_name() == "runtime_probe", "the worker keeps its own outgoing scene");

        runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
        const int snapshot = red(runtime.transition_from);
        runtime.render_outgoing();
        runtime.render_outgoing();
        check(runtime.outgoing_scene && red(runtime.transition_from) != snapshot, "the outgoing scene advances the blend's outgoing image");
        check(runtime.composer.outgoing_policy() == TransitionComposer::OutgoingPolicy::KeepMoving,
              "the worker keeps its outgoing scene moving by policy");

        clock.advance_ms(750);
        runtime.output_frame();
        runtime.render_outgoing();
        check(!runtime.outgoing_scene && !runtime.outgoing_matrix, "the outgoing scene is released when the blend is over");

        auto missing = start_command(3, "runtime_probe");
        missing["transition"] = second["transition"];
        check(start_throws(runtime, missing), "a transition from a frame the worker no longer has is rejected");
    }

    // A referenced frame the new session cannot use (another size) is rejected.
    {
        ManualClock clock;
        RenderRuntime runtime(transitions, &clock);
        runtime.start(start_command(1, "runtime_probe"));
        ++runtime.sequence;
        runtime.remember_sent_frame(std::vector<std::uint8_t>(32 * 32 * 3, 60));
        auto other = start_command(2, "runtime_probe");
        other["width"] = 16;  // a different size cannot reuse the scene
        other["height"] = 16;
        other["transition"] = {{"name", "blend"}, {"duration_ms", 750}, {"from_frame", {{"session", 1}, {"sequence", 1}}}};
        check(start_throws(runtime, other), "a referenced frame of another size is rejected");
    }

    if (failures == 0)
        std::cout << "ok\n";
    registry->remove_scene("runtime_probe");
    registry->remove_scene("runtime_probe_held");
    return failures == 0 ? 0 : 1;
}

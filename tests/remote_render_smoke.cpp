#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/remote_render.h>
#include <shared/matrix/media_artwork_state.h>

#include <emulator.h>
#include <led-matrix.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {
class RemoteProbeScene final : public Scenes::Scene {
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return "remote_probe"; }
    Scenes::SceneCapabilities get_capabilities() const override {
        auto caps = Scenes::Scene::get_capabilities();
        caps.supports_remote_rendering = true;
        return caps;
    }
protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};
}

int main()
{
    std::vector<nlohmann::json> commands;
    RemoteRender::set_command_sender([&](const nlohmann::json &command) {
        commands.push_back(command);
    });

    RemoteProbeScene scene;
    scene.update_default_properties();
    scene.register_properties();
    scene.load_properties(nlohmann::json::object());
    scene.initialize(32, 32);

    if (RemoteRender::request_scene(scene, 32, 32, 60).has_value()) {
        std::cerr << "remote scene started before a capable worker advertised itself\n";
        return 1;
    }

    RemoteRender::report_worker_heartbeat(
        RemoteRenderProtocol::Version, {"remote_probe", "another_scene"});
    if (!RemoteRender::worker_available("remote_probe")
        || RemoteRender::worker_available("missing_scene")) {
        std::cerr << "worker scene capability advertisement was not respected\n";
        return 2;
    }

    MediaArtworkState::Palette palette{};
    palette[0] = {12, 34, 56};
    palette[1] = {78, 90, 123};
    palette[2] = {145, 167, 189};
    palette[3] = {210, 45, 67};
    palette[4] = {89, 210, 34};
    MediaArtworkState::update("remote-render-smoke", palette);

    const auto session = RemoteRender::request_scene(scene, 32, 32, 60);
    if (!session.has_value() || commands.size() != 1 || commands.back().value("op", "") != "start") {
        std::cerr << "remote scene did not emit a start command\n";
        return 3;
    }
    if (commands.back().value("protocol", 0) != RemoteRenderProtocol::Version
        || commands.back().value("scene", std::string{}) != "remote_probe"
        || commands.back().contains("renderer")
        || !commands.back().value("artwork", nlohmann::json::object()).value("valid", false)) {
        std::cerr << "start command still depends on a per-scene desktop renderer\n";
        return 4;
    }

    // Identical requests reuse the active session rather than resetting the
    // desktop animation or injecting an unnecessary blank handoff.
    const auto reused = RemoteRender::request_scene(scene, 32, 32, 60);
    if (!reused.has_value() || *reused != *session || commands.size() != 1) {
        std::cerr << "identical remote scene request did not reuse its session\n";
        return 2;
    }

    rgb_matrix::RGBMatrix::Options led;
    led.rows = 32;
    led.cols = 32;
    led.chain_length = 1;
    led.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(led, emulator));
    if (!matrix) {
        std::cerr << "failed to create emulator matrix\n";
        return 3;
    }
    auto *canvas = matrix->CreateFrameCanvas();

    std::vector<std::uint8_t> frame(32 * 32 * 3);
    for (std::size_t i = 0; i < frame.size(); i += 3) {
        frame[i] = 17;
        frame[i + 1] = 83;
        frame[i + 2] = 201;
    }
    if (!RemoteRender::submit_frame(*session, 10, 32, 32, frame)) {
        std::cerr << "fresh remote frame was rejected\n";
        return 4;
    }
    if (RemoteRender::submit_frame(*session, 9, 32, 32, frame)) {
        std::cerr << "out-of-order remote frame was accepted\n";
        return 5;
    }
    if (RemoteRender::submit_frame(*session + 1, 11, 32, 32, frame)) {
        std::cerr << "stale-session remote frame was accepted\n";
        return 6;
    }
    if (!RemoteRender::copy_latest(*session, canvas, 32, 32)) {
        std::cerr << "latest remote frame was not available\n";
        return 7;
    }

    std::uint8_t r = 0, g = 0, b = 0;
    canvas->GetPixel(11, 19, &r, &g, &b);
    if (r != 17 || g != 83 || b != 201) {
        std::cerr << "remote RGB frame was copied incorrectly\n";
        return 8;
    }

    const auto reconnect = RemoteRender::reconnect_command();
    if (!reconnect.has_value() || reconnect->value("session", 0U) != *session) {
        std::cerr << "remote reconnect command did not preserve the active session\n";
        return 9;
    }

    RemoteRender::stop();
    if (commands.size() != 2 || commands.back().value("op", "") != "stop") {
        std::cerr << "remote scene did not emit a stop command\n";
        return 10;
    }
    RemoteRender::clear_command_sender();
    return 0;
}

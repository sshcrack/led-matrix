#include <shared/common/remote_render_protocol.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/remote_render.h>
#include <shared/matrix/media_artwork_state.h>
#include "../plugins/RenderOffload/matrix/RenderOffloadPlugin.h"

#ifdef ENABLE_EMULATOR
#include <emulator.h>
#include <led-matrix.h>
#endif

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
class RemoteProbeScene : public Scenes::Scene {
public:
    int contract = 1;
    int remote_contract_version() const override { return contract; }
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

int main(int argc, char **argv)
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

    RemoteRender::set_local_scene_contracts({{"remote_probe", 1}, {"another_scene", 1}});
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version);
    if (RemoteRender::worker_available("remote_probe")) {
        std::cerr << "worker without scene contracts was accepted\n";
        return 8;
    }
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version - 1, {{"remote_probe", 1}});
    if (RemoteRender::worker_available("remote_probe") || RemoteRender::status().worker_compatible) {
        std::cerr << "unsupported protocol was accepted\n";
        return 9;
    }

    // Exercise production JSON decoding with a mixed-version worker. Its old
    // source fingerprint is deliberately different and has no authority.
    RenderOffloadPlugin transport;
    transport.on_websocket_message(nlohmann::json{
        {"op", "worker_heartbeat"}, {"protocol", RemoteRenderProtocol::Version},
        {"scene_code", "completely-different-renderer-build"},
        {"scene_contracts", {{"remote_probe", 2}, {"another_scene", 1}, {"missing_scene", 1}}}
    }.dump());
    if (!RemoteRender::worker_available("another_scene") || !RemoteRender::worker_available()
        || RemoteRender::worker_available("remote_probe") || RemoteRender::worker_available("missing_scene")
        || RemoteRender::status().worker_compatible_scene_count != 1
        || RemoteRender::request_scene(scene, 32, 32, 60).has_value()) {
        std::cerr << "mixed worker contracts did not gate each requested scene independently\n";
        return 10;
    }
    for (const int invalid : {0, -1}) {
        RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, {{"remote_probe", invalid}});
        if (RemoteRender::worker_available("remote_probe")) {
            std::cerr << "nonpositive contract was accepted\n";
            return 11;
        }
    }
    for (const auto &invalid : {nlohmann::json(1.5), nlohmann::json("1"),
                                nlohmann::json(std::uint64_t{4294967297})}) {
        transport.on_websocket_message(nlohmann::json{
            {"op", "worker_heartbeat"}, {"protocol", RemoteRenderProtocol::Version},
            {"scene_contracts", {{"remote_probe", invalid}}}
        }.dump());
        if (RemoteRender::worker_available("remote_probe")) return 15;
    }
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, {{"remote_probe", 1}});
    if (!RemoteRender::worker_available("remote_probe") || RemoteRender::worker_available("another_scene")) {
        std::cerr << "missing scene contract did not fail closed\n";
        return 12;
    }
    // Registry updates take effect immediately, even before another heartbeat.
    RemoteRender::set_local_scene_contracts({{"remote_probe", 2}});
    if (RemoteRender::worker_available("remote_probe")) return 13;
    RemoteRender::set_local_scene_contracts({{"remote_probe", 1}});
    scene.contract = 2;
    if (RemoteRender::request_scene(scene, 32, 32, 60).has_value()) {
        std::cerr << "scene instance bypassed the advertised contract\n";
        return 14;
    }
    scene.contract = 1;

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
    if (RemoteRender::session_requested(0) || !RemoteRender::session_requested(*session)
        || RemoteRender::session_requested(*session + 1)) return 20;
    if (commands.back().value("protocol", 0) != RemoteRenderProtocol::Version
        || commands.back().value("scene", std::string{}) != "remote_probe"
        || commands.back().value("scene_contract", 0) != scene.remote_contract_version()
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

#ifdef ENABLE_EMULATOR
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
#endif

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
#ifdef ENABLE_EMULATOR
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
#endif

    const auto reconnect = RemoteRender::reconnect_command();
    if (!reconnect.has_value() || reconnect->value("session", 0U) != *session) {
        std::cerr << "remote reconnect command did not preserve the active session\n";
        return 9;
    }

    RemoteRender::stop();
    if (RemoteRender::session_requested(*session)) return 21;
    if (commands.size() != 2 || commands.back().value("op", "") != "stop") {
        std::cerr << "remote scene did not emit a stop command\n";
        return 10;
    }
    const nlohmann::json transition{{"name", "blend"}, {"duration_ms", 750},
        {"from", std::vector<std::uint8_t>(32 * 32 * 3, 17)}};
    const auto transitioning = RemoteRender::request_scene(scene, 32, 32, 60, transition);
    if (!transitioning || commands.back().at("transition") != transition)
        return 11;
    const auto handoff = RemoteRender::request_scene(scene, 32, 32, 60);
    if (handoff != transitioning || commands.back().value("session", 0U) != *transitioning)
        return 12;
    const auto resume = RemoteRender::reconnect_command();
    if (!resume || resume->contains("transition")) return 13;
    const auto second = RemoteRender::request_scene(scene, 32, 32, 60, transition);
    if (!second || second == transitioning) return 14;
    if (RemoteRender::session_requested(*transitioning) || !RemoteRender::session_requested(*second))
        return 22;

    // A transition warms the scene at 60 FPS. Reusing it at its native rate
    // must keep the same session/frame and update the worker without a start.
    if (!RemoteRender::submit_frame(*second, 19, 32, 32, frame)) return 26;
    const auto command_count = commands.size();
    const auto slower = RemoteRender::request_scene(scene, 32, 32, 30);
    const auto slower_resume = RemoteRender::reconnect_command();
    if (slower != second || RemoteRender::status().last_sequence != 19
        || !RemoteRender::latest_frame_age_ms(*second).has_value()
        || commands.size() != command_count + 1
        || commands.back().value("op", "") != "state"
        || commands.back().value("target_fps", 0) != 30
        || commands.back().value("session", 0U) != *second
        || !slower_resume || slower_resume->value("target_fps", 0) != 30) {
        std::cerr << "reused session did not adopt its native frame rate\n";
        return 24;
    }
    if (RemoteRender::request_scene(scene, 32, 32, 30) != second
        || commands.size() != command_count + 1) return 25;

    // The transition loop reads the newest sequence without the state lock.
    if (RemoteRender::latest_sequence(*second) != 19 || RemoteRender::latest_sequence(*second + 1) != 0)
        return 27;
#ifdef ENABLE_EMULATOR
    // The copy reports which frame it copied, so a frame arriving between a
    // sequence check and the copy is not shown twice.
    std::uint32_t copied_sequence = 0;
    if (!RemoteRender::copy_latest(*second, canvas, 32, 32, 300.0, &copied_sequence) || copied_sequence != 19)
        return 28;
#endif
    // Additive capabilities are only used when the live worker lists them.
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, {{"remote_probe", 1}},
                                          {std::string(RemoteRenderProtocol::Features::TransitionSnapshotBase64)});
    if (!RemoteRender::worker_has_feature(RemoteRenderProtocol::Features::TransitionSnapshotBase64) ||
        RemoteRender::worker_has_feature("unknown_feature"))
        return 30;
    RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, {{"remote_probe", 1}});
    if (RemoteRender::worker_has_feature(RemoteRenderProtocol::Features::TransitionSnapshotBase64))
        return 31;

    // Base64 transition snapshots round-trip every tail length and reject damage.
    for (std::size_t length = 0; length < 8; ++length) {
        std::vector<std::uint8_t> bytes(length);
        for (std::size_t i = 0; i < length; ++i) bytes[i] = static_cast<std::uint8_t>(251 - 37 * i);
        const auto encoded = RemoteRenderProtocol::encode_base64(bytes);
        const auto decoded = RemoteRenderProtocol::decode_base64(encoded);
        if (encoded.size() != (length + 2) / 3 * 4 || !decoded || *decoded != bytes) {
            std::cerr << "base64 round trip failed for " << length << " bytes\n";
            return 32;
        }
    }
    if (RemoteRenderProtocol::encode_base64(std::vector<std::uint8_t>{'M', 'a', 'n'}) != "TWFu" ||
        RemoteRenderProtocol::decode_base64("TWE=") != std::vector<std::uint8_t>{'M', 'a'})
        return 33;
    for (const char* bad : {"TWF", "TW=u", "T*Fu", "TQ=A", "=WFu"})
        if (RemoteRenderProtocol::decode_base64(bad).has_value()) {
            std::cerr << "malformed base64 '" << bad << "' was accepted\n";
            return 34;
        }

    // Exercise the real chunk receiver, including incomplete/newer frames and
    // late duplicates. A partial frame must never replace the last complete one.
    auto deliver = [&](std::uint32_t sequence, int skip = -1) {
        auto packets = RemoteRenderProtocol::make_frame_packets(*second, sequence, 32, 32, frame);
        for (int i = static_cast<int>(packets.size()) - 1; i >= 0; --i) {
            if (i == skip) continue;
            const auto payload = packets[i]->toData();
            if (!transport.on_udp_packet(RemoteRenderProtocol::PluginId, payload.data(), payload.size()))
                return false;
        }
        return true;
    };
    if (!deliver(20) || !deliver(20) || RemoteRender::status().last_sequence != 20
        || !deliver(21, 1) || RemoteRender::status().last_sequence != 20
        || !deliver(22) || !deliver(21) || RemoteRender::status().last_sequence != 22) {
        std::cerr << "chunk assembly published an incomplete, old or duplicate frame\n";
        return 16;
    }
    auto malformed_packets = RemoteRenderProtocol::make_frame_packets(*second, 23, 32, 32, frame);
    auto malformed_payload = malformed_packets.front()->toData();
    malformed_payload[16] -= 1; // Fewer chunks than needed to cover this frame.
    if (transport.on_udp_packet(RemoteRenderProtocol::PluginId,
                               malformed_payload.data(), malformed_payload.size())) {
        std::cerr << "receiver accepted a chunk count that cannot cover its frame\n";
        return 23;
    }

    if (argc > 1 && std::string_view(argv[1]) == "--benchmark-packets") {
        RemoteRender::SceneContracts contracts{{"remote_probe", 1}};
        for (int i = 0; i < 68; ++i) contracts.emplace("benchmark_scene_" + std::to_string(i), 1);
        RemoteRender::set_local_scene_contracts(contracts);
        RemoteRender::report_worker_heartbeat(RemoteRenderProtocol::Version, contracts);
        const auto benchmark_session = RemoteRender::request_scene(scene, 128, 128, 60);
        if (!benchmark_session) return 17;
        const std::vector<std::uint8_t> pixels(128 * 128 * 3, 85);
        auto packets = RemoteRenderProtocol::make_frame_packets(*benchmark_session, 1, 128, 128, pixels);
        std::vector<std::vector<std::uint8_t>> payloads;
        for (const auto &packet : packets) payloads.push_back(packet->toData());
        const auto start = std::chrono::steady_clock::now();
        constexpr std::uint32_t frames = 500;
        for (std::uint32_t sequence = 1; sequence <= frames; ++sequence) {
            for (auto &payload : payloads) {
                for (int byte = 0; byte < 4; ++byte)
                    payload[5 + byte] = static_cast<std::uint8_t>(sequence >> (24 - byte * 8));
                if (!transport.on_udp_packet(RemoteRenderProtocol::PluginId, payload.data(), payload.size()))
                    return 18;
            }
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (RemoteRender::status().last_sequence != frames) return 19;
        std::cout << "receiver: " << frames << " frames, " << payloads.size()
                  << " chunks/frame, " << elapsed << " ms total, " << elapsed / frames << " ms/frame\n";
    }
    RemoteRender::stop();
    RemoteRender::clear_command_sender();
    return 0;
}

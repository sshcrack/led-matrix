#include <Magick++.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "emulator.h"
#include "matrix-factory.h"
#include "led-matrix.h"

#include <shared/common/remote_render_protocol.h>
#include <shared/common/utils/utils.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/audio_state.h>
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/execution_mode.h>
#include <shared/matrix/media_artwork_state.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/preview.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/matrix/utils/consts.h>
#include <shared/matrix/utils/shared.h>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {
struct Args {
    std::string host;
    std::uint16_t port = 8080;
    fs::path plugin_dir;
    bool list_scenes = false;
    bool self_test = false;
};

Args parse_args(int argc, char **argv)
{
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string value(argv[i]);
        auto next = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("missing value for " + value);
            return argv[i];
        };
        if (value == "--host") args.host = next();
        else if (value == "--port") {
            const int port = std::stoi(next());
            if (port < 1 || port > 65535) throw std::runtime_error("invalid port");
            args.port = static_cast<std::uint16_t>(port);
        } else if (value == "--plugin-dir") args.plugin_dir = next();
        else if (value == "--list-scenes") args.list_scenes = true;
        else if (value == "--self-test") args.self_test = true;
        else if (value == "--help") {
            throw std::runtime_error("usage: led-matrix-scene-worker [--host <matrix>] [--port 8080] [--plugin-dir path] [--list-scenes] [--self-test]");
        } else {
            throw std::runtime_error("unknown argument: " + value);
        }
    }
    if (args.host.empty() && !args.list_scenes && !args.self_test)
        throw std::runtime_error("--host is required unless --list-scenes or --self-test is used");
    return args;
}

fs::path find_plugin_dir(const fs::path &explicit_dir)
{
    if (!explicit_dir.empty()) return explicit_dir;
    const auto exe = get_exec_dir();
    const std::vector<fs::path> candidates{
        exe / "scene_plugins",
        exe.parent_path() / "scene_plugins",
        exe.parent_path() / "lib" / "led-matrix-desktop" / "scene-plugins",
        exe.parent_path() / "lib" / "led-matrix-desktop" / "scene_plugins",
    };
    for (const auto &candidate : candidates)
        if (fs::is_directory(candidate)) return candidate;
    throw std::runtime_error("could not locate bundled scene worker plugins");
}

void set_plugin_dir_env(const fs::path &path)
{
#ifdef _WIN32
    _putenv_s("PLUGIN_DIR", path.string().c_str());
#else
    setenv("PLUGIN_DIR", path.string().c_str(), 1);
#endif
}

class DatagramSender {
public:
    DatagramSender(const std::string &host, std::uint16_t port)
    {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo *result = nullptr;
        const auto service = std::to_string(port);
        if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0 || !result)
            throw std::runtime_error("failed to resolve matrix UDP host '" + host + "'");
        socket_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
#ifdef _WIN32
        if (socket_ == INVALID_SOCKET) {
#else
        if (socket_ < 0) {
#endif
            freeaddrinfo(result);
            throw std::runtime_error("failed to create scene worker UDP socket");
        }
        std::memcpy(&address_, result->ai_addr, result->ai_addrlen);
        address_len_ = static_cast<int>(result->ai_addrlen);
        freeaddrinfo(result);
    }

    ~DatagramSender()
    {
#ifdef _WIN32
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
#else
        if (socket_ >= 0) close(socket_);
#endif
    }

    bool send(const UdpPacket &packet) const
    {
        const auto bytes = packet.toBytes();
#ifdef _WIN32
        const int sent = sendto(socket_, reinterpret_cast<const char *>(bytes.data()),
                                static_cast<int>(bytes.size()), 0,
                                reinterpret_cast<const sockaddr *>(&address_), address_len_);
        return sent == static_cast<int>(bytes.size());
#else
        const auto sent = sendto(socket_, bytes.data(), bytes.size(), 0,
                                 reinterpret_cast<const sockaddr *>(&address_), address_len_);
        return sent == static_cast<ssize_t>(bytes.size());
#endif
    }

private:
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
    sockaddr_storage address_{};
    int address_len_{};
};

std::vector<std::uint8_t> capture(rgb_matrix::FrameCanvas *canvas, int width, int height)
{
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            const auto offset = static_cast<std::size_t>((y * width + x) * 3);
            rgb[offset] = r;
            rgb[offset + 1] = g;
            rgb[offset + 2] = b;
        }
    }
    return rgb;
}

void apply_audio(const nlohmann::json &item)
{
    if (!item.is_object() || !item.value("available", false)) {
        AudioState::clear();
        return;
    }
    AudioProtocol::Frame frame;
    frame.sequence = item.value("sequence", 0U);
    frame.timestamp_ms = item.value("timestamp_ms", 0U);
    frame.flags = item.value("flags", static_cast<std::uint16_t>(0));
    frame.beat_counter = item.value("beat_counter", std::uint64_t{0});
    frame.onset_counter = item.value("onset_counter", std::uint64_t{0});
    frame.drop_counter = item.value("drop_counter", std::uint64_t{0});
    frame.section_counter = item.value("section_counter", std::uint64_t{0});
    if (const auto it = item.find("features"); it != item.end() && it->is_array()) {
        const auto count = std::min(frame.features.size(), it->size());
        for (std::size_t i = 0; i < count; ++i)
            if ((*it)[i].is_number()) frame.features[i] = (*it)[i].get<float>();
    }
    if (const auto it = item.find("spectrum"); it != item.end() && it->is_array())
        frame.spectrum = it->get<std::vector<float>>();
    if (const auto it = item.find("waveform"); it != item.end() && it->is_array())
        frame.waveform = it->get<std::vector<float>>();
    AudioState::update(frame);
}

void run_self_test(const std::vector<std::string> &available)
{
    // Exercise real scene implementations rather than only proving that their
    // DSOs load. These are deliberately CPU-heavy portable scenes from several
    // plugins; none require network data or user configuration.
    const std::vector<std::string> probes{
        "metablob", "neontunnel", "boids", "reaction_diffusion", "julia_set"
    };
    for (const auto &name : probes) {
        if (std::find(available.begin(), available.end(), name) == available.end())
            throw std::runtime_error("self-test scene is not advertised: " + name);
    }

    rgb_matrix::RGBMatrix::Options led_options;
    led_options.rows = 128;
    led_options.cols = 128;
    led_options.chain_length = 1;
    led_options.parallel = 1;
    rgb_matrix::EmulatorOptions emulator_options;
    emulator_options.headless = true;
    emulator_options.refresh_rate_hz = 60;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(
        rgb_matrix::EmulatorMatrix::Create(led_options, emulator_options));
    if (!matrix)
        throw std::runtime_error("self-test could not create headless matrix");
    auto *canvas = matrix->CreateFrameCanvas();

    for (const auto &name : probes) {
        nlohmann::json scene_json{{"type", name}, {"arguments", nlohmann::json::object()}};
        auto scene = Scenes::Scene::from_json(scene_json);
        if (!scene || scene->get_name() != name)
            throw std::runtime_error("self-test could not instantiate scene: " + name);
        scene->initialize(128, 128);
        scene->set_runtime_target_fps(60);
        for (int frame = 0; frame < 3; ++frame) {
            canvas->Clear();
            if (!scene->render_frame(canvas, 1.0 / 60.0, true))
                throw std::runtime_error("self-test scene stopped unexpectedly: " + name);
        }
        scene->after_render_stop();
    }
}

struct RenderRuntime {
    rgb_matrix::EmulatorMatrix *matrix = nullptr;
    rgb_matrix::FrameCanvas *canvas = nullptr;
    std::unique_ptr<Scenes::Scene> scene;
    std::uint32_t session = 0;
    std::uint32_t sequence = 0;
    int width = 0;
    int height = 0;
    int target_fps = 60;
    Clock::time_point next_frame{};

    ~RenderRuntime() { reset(); }

    void reset()
    {
        if (scene) {
            try { scene->after_render_stop(); } catch (...) {}
        }
        scene.reset();
        canvas = nullptr;
        delete matrix;
        matrix = nullptr;
        session = sequence = 0;
        width = height = 0;
    }

    void start(const nlohmann::json &command)
    {
        reset();
        const int protocol = command.value("protocol", 0);
        if (protocol != RemoteRenderProtocol::Version)
            throw std::runtime_error("unsupported remote render protocol");
        session = command.value("session", 0U);
        width = std::clamp(command.value("width", 128), 1, 1024);
        height = std::clamp(command.value("height", 128), 1, 1024);
        target_fps = std::clamp(command.value("target_fps", 60), 1, 60);
        if (session == 0) throw std::runtime_error("remote session id is zero");

        rgb_matrix::RGBMatrix::Options led_options;
        led_options.rows = height;
        led_options.cols = width;
        led_options.chain_length = 1;
        led_options.parallel = 1;
        rgb_matrix::EmulatorOptions emulator_options;
        emulator_options.headless = true;
        emulator_options.refresh_rate_hz = target_fps;
        matrix = rgb_matrix::EmulatorMatrix::Create(led_options, emulator_options);
        if (!matrix) throw std::runtime_error("failed to create headless scene-worker canvas");
        canvas = matrix->CreateFrameCanvas();
        canvas->Clear();

        if (const auto it = command.find("inputs"); it != command.end())
            RuntimeInputs::replace_from_json(*it);
        if (const auto it = command.find("audio"); it != command.end())
            apply_audio(*it);
        if (const auto it = command.find("artwork"); it != command.end())
            MediaArtworkState::replace_from_json(*it);

        nlohmann::json scene_json{
            {"type", command.value("scene", std::string{})},
            {"arguments", command.value("arguments", nlohmann::json::object())},
            {"variant", command.value("variant", std::string{})},
            {"uuid", command.value("uuid", std::string{})},
        };
        scene = Scenes::Scene::from_json(scene_json);
        if (!scene || scene->get_name() != command.value("scene", std::string{}))
            throw std::runtime_error("scene worker does not have requested scene");
        scene->initialize(width, height);
        scene->set_runtime_target_fps(target_fps);
        scene->restore_frame_timeline(command.value("elapsed_seconds", 0.0));
        if (const auto state = command.find("runtime_state"); state != command.end())
            scene->restore_runtime_state(*state);
        sequence = 0;
        next_frame = Clock::now();
        spdlog::info("Remote scene '{}' started (session {}, {}x{} @ {} FPS)",
                     scene->get_name(), session, width, height, target_fps);
    }
};

} // namespace

int main(int argc, char **argv)
{
    spdlog::cfg::load_env_levels();
    try {
        const auto args = parse_args(argc, argv);
        // --list-scenes is a machine-readable capability probe used by tests
        // and diagnostics. Keep stdout as a single JSON document.
        if (args.list_scenes || args.self_test)
            spdlog::set_level(spdlog::level::off);
        const auto plugin_dir = find_plugin_dir(args.plugin_dir);
        set_plugin_dir_env(plugin_dir);
        Magick::InitializeMagick(*argv);
        ix::initNetSystem();

        SceneExecution::Scope worker_runtime(SceneExecution::Mode::RemoteWorker);
        fs::create_directories(Constants::root_dir);
        Constants::width = 128;
        Constants::height = 128;
        Constants::global_post_processor = nullptr;
        Constants::global_transition_manager = nullptr;
        Constants::global_update_manager = nullptr;

        const auto temp_config = fs::temp_directory_path() / "led-matrix-scene-worker.json";
        config = new Config::MainConfig(temp_config.string());

        auto *plugins = Plugins::PluginManager::instance();
        plugins->initialize();
        auto wrappers = plugins->get_scenes();
        std::vector<Plugins::BasicPlugin *> lifecycle_plugins;
        for (auto *plugin : plugins->get_plugins()) {
            if (plugin->get_scenes().empty()) continue;
            plugin->before_server_init();
            plugin->after_server_init();
            lifecycle_plugins.push_back(plugin);
        }

        std::vector<std::string> offloadable_scenes;
        offloadable_scenes.reserve(wrappers.size());
        for (const auto &wrapper : wrappers) {
            const auto scene = wrapper->get_default();
            if (scene && scene->get_capabilities().supports_remote_rendering)
                offloadable_scenes.push_back(scene->get_name());
        }
        std::sort(offloadable_scenes.begin(), offloadable_scenes.end());
        offloadable_scenes.erase(std::unique(offloadable_scenes.begin(), offloadable_scenes.end()), offloadable_scenes.end());
        spdlog::info("Scene worker loaded {} offloadable scenes from {}", offloadable_scenes.size(), plugin_dir.string());

        const auto clean_exit = [&] {
            wrappers.clear();
            for (auto *plugin : lifecycle_plugins) plugin->pre_exit();
            config->release_scene_references();
            plugins->delete_references();
            plugins->destroy_plugins();
            delete config;
            config = nullptr;
            ix::uninitNetSystem();
        };

        if (args.list_scenes) {
            std::cout << nlohmann::json(offloadable_scenes).dump() << std::endl;
            clean_exit();
            return 0;
        }
        if (args.self_test) {
            run_self_test(offloadable_scenes);
            std::cout << R"({"ok":true,"scenes":["metablob","neontunnel","boids","reaction_diffusion","julia_set"]})" << std::endl;
            clean_exit();
            return 0;
        }

        DatagramSender udp(args.host, args.port);
        ix::WebSocket websocket;
        websocket.setUrl("ws://" + args.host + ":" + std::to_string(args.port)
                         + "/desktopWebsocket?role=scene-worker");
        websocket.enableAutomaticReconnection();

        std::mutex command_mutex;
        std::deque<nlohmann::json> commands;
        std::atomic<bool> connected{false};
        websocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr &message) {
            if (message->type == ix::WebSocketMessageType::Open) {
                connected.store(true);
                return;
            }
            if (message->type == ix::WebSocketMessageType::Close
                || message->type == ix::WebSocketMessageType::Error) {
                connected.store(false);
                return;
            }
            if (message->type != ix::WebSocketMessageType::Message)
                return;
            const std::string &payload = message->str;
            if (!payload.starts_with("msg:RenderOffload:"))
                return;
            try {
                auto command = nlohmann::json::parse(payload.substr(std::string("msg:RenderOffload:").size()));
                std::lock_guard lock(command_mutex);
                commands.push_back(std::move(command));
            } catch (const std::exception &e) {
                spdlog::debug("Ignored invalid worker command: {}", e.what());
            }
        });
        websocket.start();

        RenderRuntime runtime;
        auto next_heartbeat = Clock::now();
        while (true) {
            std::deque<nlohmann::json> pending;
            {
                std::lock_guard lock(command_mutex);
                pending.swap(commands);
            }
            for (const auto &command : pending) {
                const auto op = command.value("op", std::string{});
                const auto session = command.value("session", 0U);
                try {
                    if (op == "start") {
                        runtime.start(command);
                        Constants::width = runtime.width;
                        Constants::height = runtime.height;
                    } else if (op == "stop" && session == runtime.session) {
                        runtime.reset();
                    } else if (op == "state" && session == runtime.session) {
                        if (const auto it = command.find("inputs"); it != command.end())
                            RuntimeInputs::replace_from_json(*it);
                        if (const auto it = command.find("audio"); it != command.end())
                            apply_audio(*it);
                        if (const auto it = command.find("artwork"); it != command.end())
                            MediaArtworkState::replace_from_json(*it);
                    }
                } catch (const std::exception &e) {
                    spdlog::error("Scene worker command '{}' failed: {}", op, e.what());
                    if (op == "start") runtime.reset();
                }
            }

            const auto now = Clock::now();
            if (connected.load() && now >= next_heartbeat) {
                const nlohmann::json heartbeat{
                    {"op", "worker_heartbeat"},
                    {"protocol", RemoteRenderProtocol::Version},
                    {"scenes", offloadable_scenes},
                };
                websocket.send("msg:RenderOffload:" + heartbeat.dump());
                next_heartbeat = now + std::chrono::seconds(1);
            }

            if (runtime.scene && now >= runtime.next_frame) {
                bool keep_running = true;
                try {
                    keep_running = runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
                    const auto rgb = capture(runtime.canvas, runtime.width, runtime.height);
                    auto packets = RemoteRenderProtocol::make_frame_packets(
                        runtime.session, ++runtime.sequence,
                        static_cast<std::uint16_t>(runtime.width),
                        static_cast<std::uint16_t>(runtime.height), rgb);
                    for (const auto &packet : packets) {
                        if (!udp.send(*packet)) {
                            spdlog::debug("Scene worker UDP send failed");
                            break;
                        }
                    }
                } catch (const std::exception &e) {
                    spdlog::error("Remote scene '{}' render failed: {}", runtime.scene->get_name(), e.what());
                    keep_running = false;
                }
                const auto frame_step = std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(1.0 / static_cast<double>(runtime.target_fps)));
                runtime.next_frame = std::max(runtime.next_frame + frame_step, Clock::now());
                if (!keep_running) runtime.reset();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // The worker is normally terminated by its desktop supervisor. This is
        // intentionally unreachable, but documents the required DSO teardown order.
        runtime.reset();
        websocket.stop();
        for (auto *plugin : lifecycle_plugins) plugin->pre_exit();
        wrappers.clear();
        config->release_scene_references();
        plugins->delete_references();
        plugins->destroy_plugins();
        delete config;
        config = nullptr;
        ix::uninitNetSystem();
        return 0;
    } catch (const std::exception &e) {
        spdlog::critical("Scene worker failed: {}", e.what());
        return 1;
    }
}

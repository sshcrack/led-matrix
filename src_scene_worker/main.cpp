#include "shared/matrix/transition_manager.h"
#ifdef _WIN32
#include "shared/common/win_compat.h"
#include <cstdlib>
#endif
#include <Magick++.h>
// GraphicsMagick defines ssize_t as a macro on Windows. ixwebsocket declares
// the POSIX-compatible type itself, so let it see the identifier unexpanded.
#ifdef ssize_t
#undef ssize_t
#endif
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <shared/common/crash_reporter.h>
#include <shared/common/desktop_control_protocol.h>
#include <shared/common/remote_render_protocol.h>
#include <shared/common/utils/utils.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/audio_state.h>
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/execution_mode.h>
#include <shared/matrix/media_artwork_state.h>
#include <shared/matrix/plugin/lifecycle.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/preview.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/matrix/utils/consts.h>
#include <shared/matrix/utils/shared.h>

#include "RenderRuntime.h"
#include "emulator.h"
#include "led-matrix.h"
#include "matrix-factory.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using SceneWorker::apply_audio;
using SceneWorker::capture;
using SceneWorker::ManualClock;
using SceneWorker::RenderRuntime;

namespace {
struct Args {
    std::string host;
    std::string client_id;
    std::uint16_t port = 8080;
    fs::path plugin_dir;
    fs::path crash_dir;
    fs::path contract_fixture;
    bool list_scenes = false;
    bool self_test = false;
};


class WorkerSingletonGuard {
public:
    WorkerSingletonGuard() = default;
    WorkerSingletonGuard(const WorkerSingletonGuard&) = delete;
    WorkerSingletonGuard& operator=(const WorkerSingletonGuard&) = delete;

    ~WorkerSingletonGuard()
    {
#ifdef _WIN32
        if (mutex_) {
            ReleaseMutex(mutex_);
            CloseHandle(mutex_);
        }
#else
        if (lock_fd_ >= 0)
            close(lock_fd_);
#endif
    }

    bool acquire(const std::string& host, std::uint16_t port)
    {
        std::uint64_t key = 1469598103934665603ULL;
        for (const unsigned char c : host) {
            key ^= c;
            key *= 1099511628211ULL;
        }
        key ^= static_cast<std::uint8_t>(port & 0xffU);
        key *= 1099511628211ULL;
        key ^= static_cast<std::uint8_t>((port >> 8U) & 0xffU);
        key *= 1099511628211ULL;
        const auto key_text = std::to_string(key);
#ifdef _WIN32
        const auto mutex_name = "Local\\LedMatrixSceneWorker-" + key_text;
        mutex_ = CreateMutexA(nullptr, TRUE, mutex_name.c_str());
        if (!mutex_)
            return false;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
            return false;
        }
        return true;
#else
        const auto lock_path = fs::temp_directory_path()
            / ("led-matrix-scene-worker-" + std::to_string(static_cast<unsigned long>(getuid()))
               + "-" + key_text + ".lock");
        lock_fd_ = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (lock_fd_ < 0)
            return false;
        if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
            close(lock_fd_);
            lock_fd_ = -1;
            return false;
        }
        return true;
#endif
    }

private:
#ifdef _WIN32
    HANDLE mutex_ = nullptr;
#else
    int lock_fd_ = -1;
#endif
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
        else if (value == "--client-id") args.client_id = next();
        else if (value == "--port") {
            const int port = std::stoi(next());
            if (port < 1 || port > 65535) throw std::runtime_error("invalid port");
            args.port = static_cast<std::uint16_t>(port);
        } else if (value == "--plugin-dir") args.plugin_dir = next();
        else if (value == "--crash-dir") args.crash_dir = next();
        else if (value == "--list-scenes") args.list_scenes = true;
        else if (value == "--self-test") args.self_test = true;
        else if (value == "--contract-fixture") args.contract_fixture = next();
        else if (value == "--help") {
            throw std::runtime_error("usage: led-matrix-scene-worker [--host <matrix>] [--port 8080] [--client-id id] [--plugin-dir path] [--crash-dir path] [--list-scenes] [--self-test [--contract-fixture path]]");
        } else {
            throw std::runtime_error("unknown argument: " + value);
        }
    }
    if (!args.contract_fixture.empty() && !args.self_test)
        throw std::runtime_error("--contract-fixture requires --self-test");
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

void self_test_audio_relay()
{
    const auto relay = [](std::uint32_t sequence, bool fresh, double age_seconds) {
        apply_audio({{"available", true}, {"fresh", fresh}, {"sequence", sequence}, {"age_seconds", age_seconds}});
    };
    relay(7, true, 0.0);
    if (!AudioState::snapshot().fresh())
        throw std::runtime_error("relayed audio was not applied");
    relay(8, true, 2.0);
    if (AudioState::snapshot().fresh())
        throw std::runtime_error("relayed audio ignored the Pi-side age");
    relay(8, true, 0.0);
    if (AudioState::snapshot().fresh())
        throw std::runtime_error("a repeated audio relay renewed stale audio");
    relay(9, false, 0.0);
    if (AudioState::snapshot().fresh() || AudioState::snapshot().sequence != 8)
        throw std::runtime_error("audio the Pi marked stale was applied");
    apply_audio({{"available", false}});
    relay(8, true, 0.0);
    if (!AudioState::snapshot().fresh())
        throw std::runtime_error("audio after a restart was not applied");
    apply_audio({{"available", false}});
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

} // namespace

int main(int argc, char **argv)
{
    spdlog::cfg::load_env_levels();
    try {
        const auto args = parse_args(argc, argv);
        const auto crash_dir = args.crash_dir.empty()
            ? (fs::temp_directory_path() / "led-matrix-crashes")
            : args.crash_dir;
        CrashReporter::install({"led-matrix-scene-worker", crash_dir});
        CrashReporter::attach_to_default_logger();
        CrashReporter::set_activity("initializing scene worker");
        // --list-scenes is a machine-readable capability probe used by tests
        // and diagnostics. Keep stdout as a single JSON document.
        if (args.list_scenes || args.self_test)
            spdlog::set_level(spdlog::level::off);

        WorkerSingletonGuard singleton;
        if (!args.list_scenes && !args.self_test && !singleton.acquire(args.host, args.port)) {
            spdlog::error("Another led-matrix scene worker is already running; refusing duplicate renderer");
            return 3;
        }

        const auto plugin_dir = find_plugin_dir(args.plugin_dir);
        set_plugin_dir_env(plugin_dir);
        Magick::InitializeMagick(*argv);
        ix::initNetSystem();

        SceneExecution::Scope worker_runtime(SceneExecution::Mode::RemoteWorker);
        {
            // root_dir is <cwd>/images; the installed Windows app runs from the read-only
            // Program Files, so a failure here must not kill the worker (only image caches need it).
            std::error_code images_ec;
            fs::create_directories(Constants::root_dir, images_ec);
            if (images_ec)
                spdlog::warn("Scene worker: cannot create '{}': {}", Constants::root_dir.string(), images_ec.message());
        }
        Constants::width = 128;
        Constants::height = 128;
        TransitionManager transition_manager;

        const auto temp_config = fs::temp_directory_path() / ("led-matrix-scene-worker-" + std::to_string(args.port) + ".json");
        auto* config = new Config::MainConfig(temp_config.string());

        auto *plugins = Plugins::PluginManager::instance();
        plugins->initialize();
        plugins->attach_host_config(config);
        for (auto *plugin : plugins->get_plugins())
            for (auto &effect : plugin->create_transitions())
                transition_manager.register_transition(std::move(effect));
        auto wrappers = plugins->get_scenes();
        std::vector<Plugins::BasicPlugin *> lifecycle_plugins;
        for (auto *plugin : plugins->get_plugins()) {
            if (plugin->get_scenes().empty()) continue;
            // Track the plugin before running its hooks so a failure still
            // runs pre_exit for it and everything initialised before it.
            lifecycle_plugins.push_back(plugin);
            for (const auto stage : {Plugins::LifecycleStage::BeforeServerInit, Plugins::LifecycleStage::AfterServerInit}) {
                if (const auto failure = Plugins::run_lifecycle_stage(plugin, stage)) {
                    for (const auto &exit_failure : Plugins::run_lifecycle_stage_all(lifecycle_plugins, Plugins::LifecycleStage::PreExit))
                        spdlog::error("{}", exit_failure.describe());
                    throw std::runtime_error(failure->describe());
                }
            }
        }

        std::vector<std::string> offloadable_scenes;
        nlohmann::json offloadable_scene_contracts = nlohmann::json::object();
        offloadable_scenes.reserve(wrappers.size());
        for (const auto &wrapper : wrappers) {
            const auto scene = wrapper->get_default();
            if (scene && scene->get_capabilities().supports_remote_rendering) {
                const auto name = scene->get_name();
                const int contract = scene->remote_contract_version();
                if (contract <= 0)
                    throw std::runtime_error("Invalid remote contract for scene: " + name);
                offloadable_scenes.push_back(name);
                offloadable_scene_contracts[name] = contract;
            }
        }
        std::sort(offloadable_scenes.begin(), offloadable_scenes.end());
        offloadable_scenes.erase(std::unique(offloadable_scenes.begin(), offloadable_scenes.end()), offloadable_scenes.end());
        spdlog::info("Scene worker loaded {} offloadable scenes from {}", offloadable_scenes.size(), plugin_dir.string());

        const auto clean_exit = [&] {
            wrappers.clear();
            for (const auto &failure : Plugins::run_lifecycle_stage_all(lifecycle_plugins, Plugins::LifecycleStage::PreExit))
                spdlog::error("{}", failure.describe());
            config->release_scene_references();
            plugins->delete_references();
            transition_manager.clear();
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
            if (!args.contract_fixture.empty()) {
                std::ifstream input(args.contract_fixture);
                const auto fixture = nlohmann::json::parse(input);
                for (const auto &field : {"protocol", "scene_contract"}) {
                    for (const auto &value : {nlohmann::json(), nlohmann::json(99)}) {
                        auto invalid = fixture;
                        if (value.is_null()) invalid.erase(field);
                        else invalid[field] = value;
                        RenderRuntime rejected(transition_manager);
                        bool threw = false;
                        try { rejected.start(invalid); }
                        catch (const std::exception &) { threw = true; }
                        if (!threw || rejected.scene || rejected.matrix)
                            throw std::runtime_error("worker accepted incompatible contract fixture");
                    }
                }
                RenderRuntime accepted(transition_manager);
                accepted.start(fixture);
                if (!accepted.scene->render_frame(accepted.canvas, 1.0 / 30.0, true))
                    throw std::runtime_error("saved contract fixture stopped rendering");
                const auto pixels = capture(accepted.output_frame(), accepted.width, accepted.height);
                if (std::none_of(pixels.begin(), pixels.end(), [](auto pixel) { return pixel != 0; }))
                    throw std::runtime_error("saved contract fixture produced no pixels");
            }
            run_self_test(offloadable_scenes);
            self_test_audio_relay();
            if (transition_manager.get_registered_transitions().empty())
                throw std::runtime_error("worker has no registered transitions");
            bool base64_snapshot = false;
            for (const auto &name : transition_manager.get_registered_transitions()) {
                ManualClock clock;
                RenderRuntime probe(transition_manager, &clock);
                // Alternate both snapshot encodings: older matrices still send the array.
                const std::vector<std::uint8_t> snapshot(128 * 128 * 3, 31);
                nlohmann::json transition{{"name", name}, {"duration_ms", 750}};
                if ((base64_snapshot = !base64_snapshot))
                    transition["from_base64"] = RemoteRenderProtocol::encode_base64(snapshot);
                else
                    transition["from"] = snapshot;
                probe.start({{"protocol", RemoteRenderProtocol::Version},
                             {"session", 1},
                             {"width", 128},
                             {"height", 128},
                             {"scene", "boids"},
                             {"scene_contract", offloadable_scene_contracts.at("boids")},
                             {"transition", transition}});
                std::array<std::uint8_t, 3> from{};
                probe.transition_from->GetPixel(5, 7, &from[0], &from[1], &from[2]);
                if (from != std::array<std::uint8_t, 3>{31, 31, 31})
                    throw std::runtime_error("worker decoded a wrong transition snapshot: " + name);
                probe.scene->render_frame(probe.canvas, 1.0 / 60.0, true);
                const auto *original_scene = probe.scene.get();
                const auto elapsed = probe.scene->get_frame_context().elapsed_seconds;
                probe.sequence = 42;
                probe.apply_state({{"target_fps", 30}});
                if (probe.target_fps != 30 || probe.scene->get_declared_target_fps() != 30 || probe.scene.get() != original_scene ||
                    probe.session != 1 || probe.sequence != 42 || probe.scene->get_frame_context().elapsed_seconds != elapsed ||
                    !probe.composer.active())
                    throw std::runtime_error("rate update restarted the warmed scene");
                probe.apply_state(nlohmann::json::object());
                if (probe.target_fps != 30)
                    throw std::runtime_error("legacy state message changed the worker rate");
                probe.canvas->Fill(210, 120, 80);
                clock.advance_ms(375);
                if (probe.output_frame() != probe.transition_output)
                    throw std::runtime_error("worker skipped transition: " + name);
                clock.advance_ms(625);
                if (probe.output_frame() != probe.canvas || probe.composer.active())
                    throw std::runtime_error("worker failed transition handoff: " + name);
            }
            {  // A transition can start from a frame the worker sent itself.
                ManualClock clock;
                RenderRuntime probe(transition_manager, &clock);
                const nlohmann::json boids{{"protocol", RemoteRenderProtocol::Version},
                                           {"width", 128},
                                           {"height", 128},
                                           {"scene", "boids"},
                                           {"scene_contract", offloadable_scene_contracts.at("boids")}};
                auto first = boids;
                first["session"] = 1;
                probe.start(first);
                for (std::uint8_t value : {40, 41, 42}) {
                    ++probe.sequence;
                    probe.remember_sent_frame(std::vector<std::uint8_t>(128 * 128 * 3, value));
                }
                auto second = boids;
                second["session"] = 2;
                second["transition"] = {{"name", "blend"}, {"duration_ms", 750}, {"from_frame", {{"session", 1}, {"sequence", 2}}}};
                probe.start(second);
                std::array<std::uint8_t, 3> from{};
                probe.transition_from->GetPixel(5, 7, &from[0], &from[1], &from[2]);
                if (probe.session != 2 || from != std::array<std::uint8_t, 3>{41, 41, 41} || !probe.sent_frames.empty())
                    throw std::runtime_error("worker did not start from the referenced frame");
                // The outgoing scene was this worker's own: it keeps animating under the blend.
                if (!probe.outgoing_scene || probe.outgoing_scene->get_name() != "boids")
                    throw std::runtime_error("worker froze its own outgoing scene");
                const auto snapshot = capture(probe.transition_from, 128, 128);
                for (int frame = 0; frame < 3; ++frame) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    probe.scene->render_frame(probe.canvas, std::nullopt, true);
                    probe.render_outgoing();
                }
                if (!probe.outgoing_scene || capture(probe.transition_from, 128, 128) == snapshot)
                    throw std::runtime_error("worker did not advance the outgoing scene during the blend");
                clock.advance_ms(1000);
                probe.output_frame();
                probe.render_outgoing();
                if (probe.composer.active() || probe.outgoing_scene || probe.outgoing_matrix)
                    throw std::runtime_error("worker kept the outgoing scene after the blend");
                auto missing = boids;
                missing["session"] = 3;
                missing["transition"] = second["transition"];
                bool rejected = false;
                try {
                    probe.start(missing);
                }
                catch (const std::runtime_error&) {
                    rejected = true;
                }
                if (!rejected)
                    throw std::runtime_error("worker accepted a transition from a frame it no longer has");
            }
            std::cout << R"({"ok":true,"scenes":["metablob","neontunnel","boids","reaction_diffusion","julia_set"]})" << std::endl;
            clean_exit();
            return 0;
        }

        DatagramSender udp(args.host, args.port);
        ix::WebSocket websocket;
        websocket.setUrl("ws://" + args.host + ":" + std::to_string(args.port)
                         + "/desktopWebsocket?role=scene-worker&client_id=" + args.client_id);
        if (const char *token = std::getenv("LED_MATRIX_API_TOKEN"); token && *token)
            websocket.setExtraHeaders({{"Authorization", std::string("Bearer ") + token}});
        websocket.enableAutomaticReconnection();

        std::mutex command_mutex;
        std::deque<nlohmann::json> commands;
        std::atomic<bool> connected{false};
        // Default active on Open for compatibility with older matrix servers;
        // current servers immediately send an explicit producer role.
        std::atomic<bool> producer_active{false};
        websocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr &message) {
            if (message->type == ix::WebSocketMessageType::Open) {
                connected.store(true);
                producer_active.store(true);
                return;
            }
            if (message->type == ix::WebSocketMessageType::Close
                || message->type == ix::WebSocketMessageType::Error) {
                connected.store(false);
                producer_active.store(false);
                return;
            }
            if (message->type != ix::WebSocketMessageType::Message)
                return;
            const std::string &payload = message->str;
            if (const auto role = DesktopControlProtocol::parse_desktop_producer(payload); role.has_value()) {
                producer_active.store(*role);
                return;
            }
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

        RenderRuntime runtime(transition_manager);
        auto next_heartbeat = Clock::now();
        bool was_producer_active = false;
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
                        runtime.apply_state(command);
                    }
                } catch (const std::exception &e) {
                    spdlog::error("Scene worker command '{}' failed: {}", op, e.what());
                    if (op == "start") runtime.reset();
                }
            }

            const auto now = Clock::now();
            const bool active_producer = connected.load() && producer_active.load();
            if (!active_producer) {
                if (was_producer_active)
                    runtime.reset();
                was_producer_active = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (!was_producer_active) {
                was_producer_active = true;
                next_heartbeat = now;
            }

            if (now >= next_heartbeat) {
                const nlohmann::json heartbeat{
                    {"op", "worker_heartbeat"},
                    {"protocol", RemoteRenderProtocol::Version},
                    {"scenes", offloadable_scenes},
                    {"scene_contracts", offloadable_scene_contracts},
                    {"features",
                     {RemoteRenderProtocol::Features::TransitionSnapshotBase64, RemoteRenderProtocol::Features::TransitionFromFrame}},
                };
                websocket.send("msg:RenderOffload:" + heartbeat.dump());
                next_heartbeat = now + std::chrono::seconds(1);
            }

            if (runtime.scene && now >= runtime.next_frame) {
                bool keep_running = true;
                try {
                    keep_running = runtime.scene->render_frame(runtime.canvas, std::nullopt, true);
                    runtime.render_outgoing();
                    const auto rgb = capture(runtime.output_frame(), runtime.width, runtime.height);
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
                    runtime.remember_sent_frame(rgb);
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
        for (const auto &failure : Plugins::run_lifecycle_stage_all(lifecycle_plugins, Plugins::LifecycleStage::PreExit))
                spdlog::error("{}", failure.describe());
        wrappers.clear();
        config->release_scene_references();
        plugins->delete_references();
        transition_manager.clear();
        plugins->destroy_plugins();
        delete config;
        config = nullptr;
        ix::uninitNetSystem();
        return 0;
    } catch (const std::exception &e) {
        CrashReporter::report_exception("scene worker fatal exception", e.what());
        spdlog::critical("Scene worker failed: {}", e.what());
        // --list-scenes / --self-test switch logging off to keep stdout as JSON;
        // still say why the probe failed so a test log shows it.
        if (!spdlog::should_log(spdlog::level::critical))
            std::cerr << "Scene worker failed: " << e.what() << std::endl;
        return 1;
    }
}

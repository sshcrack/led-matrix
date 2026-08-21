#include "daemon.h"

#include <Magick++.h>
#ifdef ENABLE_EMULATOR
#include <CLI/CLI.hpp>
#endif

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/consts.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/post_processor.h"
#include "shared/matrix/transition_manager.h"
#include "shared/matrix/update/UpdateManager.h"
#include "shared/matrix/config/MainConfig.h"
#include "matrix-factory.h"
#include "server/server.h"
#include "server/live_frame_ws.h"
#include "udp.h"
#include "matrix_control/hardware.h"

using namespace spdlog;
using namespace std;
using json = nlohmann::json;
using Plugins::PluginManager;

namespace {
    constexpr uint16_t default_http_port = 8080;
}

// ---------------------------------------------------------------------------
// Emulator-only helpers
// ---------------------------------------------------------------------------
#ifdef ENABLE_EMULATOR

void Daemon::parse_emulator_options(int argc, char* argv[])
{
    std::vector<std::string> raw_props;

    {
        CLI::App cli{"LED Matrix emulator"};
        cli.allow_extras(true);

        cli.add_option("--scene", pinned_scene_name_,
            "Pin the emulator to a single scene by name (skips normal playlist).");
        cli.add_option("--prop", raw_props,
            "Override a scene property: --prop speed=1.5  (repeatable; values are JSON-parsed).");

        try {
            cli.parse(argc, argv);
        } catch (const CLI::ParseError &e) {
            throw runtime_error(cli.exit(e) == 0
                ? "CLI parse requested exit"
                : "CLI parse error");
        }
    }

    prop_overrides_ = json::object();
    for (const auto &kv : raw_props) {
        const auto eq = kv.find('=');
        if (eq == string::npos) {
            warn("[--prop] Ignoring '{}': expected 'key=value' format", kv);
            continue;
        }
        const string key   = kv.substr(0, eq);
        const string value = kv.substr(eq + 1);
        try {
            prop_overrides_[key] = json::parse(value);
        } catch (const json::parse_error &) {
            prop_overrides_[key] = value;
        }
    }
}

std::shared_ptr<Scenes::Scene>
Daemon::build_pinned_scene()
{
    const int w = matrix_->width();
    const int h = matrix_->height();
    for (const auto &wrapper : PluginManager::instance()->get_scenes()) {
        if (wrapper->get_name() != pinned_scene_name_)
            continue;
        auto scene = wrapper->create();
        scene->update_default_properties();
        scene->register_properties();

        json props = json::object();
        for (const auto &p : scene->get_properties())
            p->dump_to_json(props);
        for (auto it = prop_overrides_.begin(); it != prop_overrides_.end(); ++it)
            props[it.key()] = it.value();

        scene->load_properties(props);
        scene->initialize(w, h);
        info("[emulator] Pinned to scene '{}'", pinned_scene_name_);
        return scene;
    }

    error("[emulator] Scene '{}' not found. Available scenes:", pinned_scene_name_);
    for (const auto &wrapper : PluginManager::instance()->get_scenes())
        error("  - {}", wrapper->get_name());
    return nullptr;
}

#endif // ENABLE_EMULATOR

// ---------------------------------------------------------------------------
// Constructor — full initialisation
// ---------------------------------------------------------------------------
Daemon::Daemon(int argc, char* argv[])
{
    try {
    // -------------------------------------------------------------------
    // 1. Magick
    // -------------------------------------------------------------------
    Magick::InitializeMagick(*argv);
    constexpr size_t magick_memory_limit = 256ULL * 1024 * 1024;
    constexpr size_t magick_map_limit = 512ULL * 1024 * 1024;
    SetMagickResourceLimit(Magick::MemoryResource, magick_memory_limit);
    SetMagickResourceLimit(Magick::MapResource, magick_map_limit);
    cfg::load_env_levels();

    // -------------------------------------------------------------------
    // 2. Emulator-only CLI parsing (consumes --scene / --prop ahead
    //    of rgb-matrix flags so CLI11 eats its own first).
    // -------------------------------------------------------------------
#ifdef ENABLE_EMULATOR
    parse_emulator_options(argc, argv);
#endif

    // -------------------------------------------------------------------
    // 3. Detect --debugger flag
    // -------------------------------------------------------------------
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--debugger") == 0)
            is_debugging_ = true;
    }

    // -------------------------------------------------------------------
    // 4. Parse rgb-matrix flags and create the hardware
    // -------------------------------------------------------------------
    rgb_matrix::MatrixFactory::Options options;
    options.runtime_options.drop_priv_user = getenv("SUDO_UID");
    options.runtime_options.drop_priv_group = getenv("SUDO_GID");

    if (!rgb_matrix::MatrixFactory::ParseOptionsFromFlags(&argc, &argv, &options)) {
        rgb_matrix::MatrixFactory::PrintMatrixFactoryFlags(stderr);
        throw runtime_error("Failed to parse matrix options");
    }

    matrix_.reset(rgb_matrix::MatrixFactory::CreateMatrix(options));
    if (!matrix_)
        throw runtime_error("Failed to create matrix");

    // -------------------------------------------------------------------
    // 5. Ensure root data directory exists
    // -------------------------------------------------------------------
    if (!filesystem::exists(Constants::root_dir))
        filesystem::create_directory(Constants::root_dir);

    // -------------------------------------------------------------------
    // 6. Global service objects
    // -------------------------------------------------------------------
    post_processor_ = make_unique<PostProcessor>();
    Constants::global_post_processor = post_processor_.get();
    info("Post-processor initialized");

    transition_manager_ = make_unique<TransitionManager>();
    Constants::global_transition_manager = transition_manager_.get();
    info("Transition manager initialized");

    // -------------------------------------------------------------------
    // 7. Plugin loader
    // -------------------------------------------------------------------
    debug("Loading plugins...");
    PluginManager::instance()->initialize();

    // -------------------------------------------------------------------
    // 8. Configuration
    // -------------------------------------------------------------------
    debug("Loading config...");
#ifndef LED_MATRIX_DATA_DIR
#define LED_MATRIX_DATA_DIR "."
#endif
    config_ = make_unique<Config::MainConfig>(
        string(LED_MATRIX_DATA_DIR) + "/config.json");
    ::config = config_.get();

    // -------------------------------------------------------------------
    // 9. Update manager
    // -------------------------------------------------------------------
    debug("Initializing UpdateManager...");
    update_manager_ = make_shared<Update::UpdateManager>(config_.get());
    Constants::global_update_manager = update_manager_;
    update_manager_->start();
    info("UpdateManager initialized and started");
    update_manager_->check_and_handle_update_completion();

    // -------------------------------------------------------------------
    // 10. Plugin before-server init + register effects / transitions
    // -------------------------------------------------------------------
    for (const auto &item : PluginManager::instance()->get_plugins()) {
        const auto err = item->before_server_init();
        if (err.has_value()) {
            error(err.value());
            throw runtime_error(err.value());
        }

        auto effects = item->create_effects();
        for (auto &effect : effects)
            Constants::global_post_processor->register_effect(move(effect));

        auto transitions = item->create_transitions();
        for (auto &transition : transitions)
            Constants::global_transition_manager->register_transition(move(transition));
    }

    info("Loaded {} Scenes and {} Image Types",
         PluginManager::instance()->get_scenes().size(),
         PluginManager::instance()->get_image_providers().size());

    // -------------------------------------------------------------------
    // 11. PORT environment
    // -------------------------------------------------------------------
    port_ = default_http_port;
    if (const char* port_env = getenv("PORT")) {
        try {
            int parsed = stoi(port_env);
            if (parsed < 0 || parsed > 65535) {
                warn("PORT env value '{}' out of range [0,65535]. Using default {}.", port_env, default_http_port);
                parsed = default_http_port;
            }
            port_ = static_cast<uint16_t>(parsed);
        } catch (const exception& e) {
            warn("Invalid PORT env value '{}': {}. Using default {}.", port_env, e.what(), default_http_port);
        }
    }

    // -------------------------------------------------------------------
    // 12. Emulator pinned scene
    // -------------------------------------------------------------------
#ifdef ENABLE_EMULATOR
    if (!pinned_scene_name_.empty()) {
        pinned_scene_ = build_pinned_scene();
        if (!pinned_scene_)
            throw runtime_error(
                fmt::format("[emulator] Scene '{}' not found", pinned_scene_name_));
    }
#endif

    // -------------------------------------------------------------------
    // 13. HTTP server + control thread
    // -------------------------------------------------------------------
    string host = "0.0.0.0";
    http_server_ = make_unique<server_t>(
        restinio::own_io_context(),
        [this, host](auto &settings)
        {
            unique_lock lock(Server::registryMutex);
            auto router = Server::server_handler(Server::registry);
            if (is_debugging_)
                router->http_get("/exit_debug", [](auto req, auto)
                {
                    interrupt_received = true;
                    exit_canvas_update = true;
                    return req->create_response().set_body("Sent interrupt.").done();
                });

            settings.port(port_);
            settings.address(host);
            settings.request_handler(move(router));
            settings.write_http_response_timelimit(30s);
            settings.cleanup_func([]
            {
                {
                    unique_lock lock(Server::registryMutex);
                    Server::registry.clear();
                }
                Server::clear_live_frame_connections();
            });
        });

    control_thread_ = thread([this, host]
    {
        info("Listening on http://{}:{}/", host, port_);
        restinio::run(restinio::on_thread_pool(
            1,
            restinio::skip_break_signal_handling(),
            *http_server_));
    });

    // -------------------------------------------------------------------
    // 14. Plugin after-server init
    // -------------------------------------------------------------------
    for (const auto &item : PluginManager::instance()->get_plugins()) {
        const auto err = item->after_server_init();
        if (err.has_value()) {
            error(err.value());
            throw runtime_error(err.value());
        }
    }

    // -------------------------------------------------------------------
    // 15. UDP pixel receiver
    // -------------------------------------------------------------------
    debug("Starting UDP server on port {}", port_);
    udp_server_ = make_unique<UdpServer>(port_);
    } catch (...) {
        // Constructors do not invoke ~Daemon() when they throw. Tear down any
        // plugin threads, HTTP/UDP workers and globals established so far.
        shutdown(false);
        throw;
    }
}

// ---------------------------------------------------------------------------
// Destructor — teardown in safe order
// ---------------------------------------------------------------------------
void Daemon::shutdown(bool persist_config) noexcept
{
    if (shutdown_started_)
        return;
    shutdown_started_ = true;

    try {
        if (http_server_)
            initiate_shutdown(*http_server_);
    } catch (const std::exception &e) {
        error("HTTP shutdown failed: {}", e.what());
    } catch (...) {
        error("HTTP shutdown failed with unknown exception");
    }

    // UdpServer owns a worker thread; resetting it joins that worker first.
    try { udp_server_.reset(); }
    catch (...) { error("UDP shutdown failed"); }

    auto *pl = PluginManager::instance();
    for (const auto &plugin : pl->get_plugins()) {
        if (!plugin) continue;
        try {
            if (auto err = plugin->pre_exit(); err.has_value()) error(err.value());
        } catch (const std::exception &e) {
            error("Plugin '{}' pre_exit failed: {}", plugin->get_plugin_name(), e.what());
        } catch (...) {
            error("Plugin pre_exit failed with unknown exception");
        }
    }

    if (persist_config && config_) {
        try { config_->save(); }
        catch (const std::exception &e) { error("Saving config during shutdown failed: {}", e.what()); }
        catch (...) { error("Saving config during shutdown failed"); }
    }

    if (control_thread_.joinable()) {
        try { control_thread_.join(); }
        catch (...) { error("Joining HTTP control thread failed"); }
    }

    // Routers contain callbacks/lambdas supplied by plugins. Destroy the HTTP
    // server and those closures before unloading any plugin DSO.
    http_server_.reset();

    if (update_manager_) {
        try { update_manager_->stop(); }
        catch (...) { error("Stopping update manager failed"); }
        update_manager_.reset();
    }

#ifdef ENABLE_EMULATOR
    // A pinned scene is a plugin object too; release it before dlclose.
    pinned_scene_.reset();
#endif
    if (config_) {
        try { config_->release_scene_references(); }
        catch (...) { error("Releasing configured scene references failed"); }
    }

    // Post-processing and transition managers may own polymorphic objects
    // implemented inside plugin DSOs. Destroy those objects while the code is
    // still loaded, then clear their global aliases.
    Constants::global_post_processor = nullptr;
    Constants::global_transition_manager = nullptr;
    post_processor_.reset();
    transition_manager_.reset();

    // External wrappers and configured scene instances must disappear before
    // their plugin DSOs are unloaded.
    try { pl->delete_references(); }
    catch (...) { error("Releasing scene wrapper references failed"); }
    try { pl->destroy_plugins(); }
    catch (...) { error("Destroying plugins failed"); }

    Constants::global_update_manager.reset();
    ::config = nullptr;
}

Daemon::~Daemon()
{
    shutdown(true);
}

// ---------------------------------------------------------------------------
// run — blocking hardware mainloop
// ---------------------------------------------------------------------------
int Daemon::run()
{
#ifdef ENABLE_EMULATOR
    return start_hardware_mainloop(matrix_.get(), pinned_scene_);
#else
    return start_hardware_mainloop(matrix_.get());
#endif
}

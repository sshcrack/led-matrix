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
                unique_lock lock(Server::registryMutex);
                Server::registry.clear();
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
}

// ---------------------------------------------------------------------------
// Destructor — teardown in safe order
// ---------------------------------------------------------------------------
Daemon::~Daemon()
{
    // 1. Signal HTTP server to stop (lets the control thread exit)
    if (http_server_)
        initiate_shutdown(*http_server_);

    // 2. Delete UDP server (its destructor joins the internal thread)
    udp_server_.reset();

    // 3. Plugin pre-exit notifications
    auto* pl = PluginManager::instance();
    for (const auto &plugin : pl->get_plugins()) {
        if (auto err = plugin->pre_exit(); err.has_value())
            error(err.value());
    }

    // 4. Persist config
    if (config_)
        config_->save();

    // 5. Join HTTP control thread (now that shutdown was signalled)
    if (control_thread_.joinable())
        control_thread_.join();

    // 6. Destroy plugins (config and globals still alive here)
    pl->destroy_plugins();
    pl->delete_references();

    // 7. Stop update manager
    if (update_manager_) {
        update_manager_->stop();
        update_manager_.reset();
    }

    // 8. Clear global pointers (owned memory freed by member destructors)
    Constants::global_post_processor = nullptr;
    Constants::global_transition_manager = nullptr;
    Constants::global_update_manager.reset();
    ::config = nullptr;

    // 9. Member destructors fire in reverse declaration order:
    //    udp_server_           (already reset — no-op)
    //    control_thread_       (already joined — no-op)
    //    http_server_          (unique_ptr deletes server_t)
    //    update_manager_       (shared_ptr, already reset — no-op)
    //    config_               (unique_ptr deletes MainConfig)
    //    transition_manager_   (unique_ptr deletes TransitionManager)
    //    post_processor_       (unique_ptr deletes PostProcessor)
    //    matrix_               (unique_ptr deletes RGBMatrixBase)
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

#include "spdlog/spdlog.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/post_processor.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"
#include <nlohmann/json.hpp>
#include <utility>

#include <Magick++.h>
#include <shared/matrix/utils/consts.h>

#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"
#include "matrix-factory.h"
#include "server/server.h"
#include "server/update_routes.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/update/UpdateManager.h"
#include "udp.h"
#include "shared/matrix/server/common.h"

#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared/matrix/canvas_consts.h>
#include "shared/matrix/transition_manager.h"

#ifdef ENABLE_EMULATOR
#include <CLI/CLI.hpp>
#endif

using namespace spdlog;
using namespace std;
using json = nlohmann::json;
using Plugins::PluginManager;

using server_t = restinio::http_server_t<Server::traits_t>;

#include "shared/matrix/utils/consts.h"

// ---------------------------------------------------------------------------
// Emulator-only helpers
// ---------------------------------------------------------------------------
#ifdef ENABLE_EMULATOR

/// Build a fully-initialised Scene from a SceneWrapper, applying default
/// properties and then any JSON overrides supplied via --prop.
static std::shared_ptr<Scenes::Scene>
build_pinned_scene(const std::shared_ptr<Plugins::SceneWrapper> &wrapper,
                   const nlohmann::json &prop_overrides,
                   int width, int height)
{
    auto scene = wrapper->create();  // fresh instance
    scene->update_default_properties();
    scene->register_properties();

    // Dump defaults then layer the CLI overrides on top.
    nlohmann::json props = nlohmann::json::object();
    for (const auto &p : scene->get_properties())
        p->dump_to_json(props);
    for (auto it = prop_overrides.begin(); it != prop_overrides.end(); ++it)
        props[it.key()] = it.value();

    scene->load_properties(props);
    scene->initialize(width, height);
    return scene;
}

#endif // ENABLE_EMULATOR

int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::MatrixFactory::PrintMatrixFactoryFlags(stderr);
    return 1;
}

int main(int argc, char *argv[])
{
    Magick::InitializeMagick(*argv);

    SetMagickResourceLimit(Magick::MemoryResource, 256 * 1024 * 1024); // Limit to 256MB
    SetMagickResourceLimit(Magick::MapResource, 512 * 1024 * 1024);    // Limit to 512MB
    cfg::load_env_levels();

    // -----------------------------------------------------------------------
    // Emulator-only: parse --scene / --prop before the rgb-matrix flags so
    // that CLI11 consumes its arguments first and the remainder is handed to
    // ParseOptionsFromFlags.
    // -----------------------------------------------------------------------
#ifdef ENABLE_EMULATOR
    std::string pinned_scene_name;
    std::vector<std::string> raw_props;  // "key=json_value" pairs

    {
        CLI::App cli{"LED Matrix emulator"};
        cli.allow_extras(true);  // pass unknown flags to rgb-matrix

        cli.add_option("--scene", pinned_scene_name,
            "Pin the emulator to a single scene by name (skips normal playlist).");
        cli.add_option("--prop", raw_props,
            "Override a scene property: --prop speed=1.5  (repeatable; values are JSON-parsed).");

        // Parse only the args CLI11 knows; leave the rest for rgb-matrix.
        try {
            cli.parse(argc, argv);
        } catch (const CLI::ParseError &e) {
            return cli.exit(e);
        }
    }

    // Build a JSON object from the raw key=value pairs.
    nlohmann::json prop_overrides = nlohmann::json::object();
    for (const auto &kv : raw_props) {
        const auto eq = kv.find('=');
        if (eq == std::string::npos) {
            spdlog::warn("[--prop] Ignoring '{}': expected 'key=value' format", kv);
            continue;
        }
        const std::string key   = kv.substr(0, eq);
        const std::string value = kv.substr(eq + 1);
        try {
            prop_overrides[key] = nlohmann::json::parse(value);
        } catch (const nlohmann::json::parse_error &) {
            // Fall back to treating the value as a plain string.
            prop_overrides[key] = value;
        }
    }
#endif // ENABLE_EMULATOR

    rgb_matrix::MatrixFactory::Options options;

    // Should be in hardware.cpp but this actually drops privileges, so I moved it here


    bool is_debugging = false;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--debugger") == 0)
            is_debugging = true;
    }


    debug("Parsing rgb matrix options from command line...");
    options.runtime_options.drop_priv_user = getenv("SUDO_UID");
    options.runtime_options.drop_priv_group = getenv("SUDO_GID");

    if (!rgb_matrix::MatrixFactory::ParseOptionsFromFlags(&argc, &argv, &options))
    {
        return usage(argv[0]);
    }

    rgb_matrix::RGBMatrixBase *matrix = rgb_matrix::MatrixFactory::CreateMatrix(options);
    if (matrix == nullptr)
        return usage(argv[0]);

    if (!filesystem::exists(Constants::root_dir))
    {
        filesystem::create_directory(Constants::root_dir);
    }

    Constants::global_post_processor = new PostProcessor();
    spdlog::info("Post-processor initialized");

    Constants::global_transition_manager = new TransitionManager();
    spdlog::info("Transition manager initialized");

    debug("Loading plugins...");
    const auto pl = PluginManager::instance();
    pl->initialize();

    debug("Loading config...");
    config = new Config::MainConfig("config.json");

    debug("Initializing UpdateManager...");
    Constants::global_update_manager = new Update::UpdateManager(config);
    Constants::global_update_manager->start();
    info("UpdateManager initialized and started");

    // Check for completed updates from previous session
    debug("Checking for completed updates...");
    Constants::global_update_manager->check_and_handle_update_completion();

    for (const auto &item : pl->get_plugins())
    {
        const auto err = item->before_server_init();
        if (err.has_value())
        {
            error(err.value());
            std::exit(-1);
        }

        auto effects = item->create_effects();
        for (auto &effect : effects)
        {
            Constants::global_post_processor->register_effect(std::move(effect));
        }

        auto transitions = item->create_transitions();
        for (auto &transition : transitions)
        {
            Constants::global_transition_manager->register_transition(std::move(transition));
        }
    }

    info("Loaded {} Scenes and {} Image Types", pl->get_scenes().size(), pl->get_image_providers().size());

    debug("Starting mainloop_thread");
    uint16_t port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;

    // -----------------------------------------------------------------------
    // Emulator-only: find and pre-build the pinned scene if --scene was given.
    // -----------------------------------------------------------------------
#ifdef ENABLE_EMULATOR
    std::shared_ptr<Scenes::Scene> pinned_scene;
    if (!pinned_scene_name.empty()) {
        const int w = matrix->width();
        const int h = matrix->height();
        for (const auto &wrapper : pl->get_scenes()) {
            if (wrapper->get_name() == pinned_scene_name) {
                pinned_scene = build_pinned_scene(wrapper, prop_overrides, w, h);
                info("[emulator] Pinned to scene '{}'", pinned_scene_name);
                break;
            }
        }
        if (!pinned_scene) {
            error("[emulator] Scene '{}' not found. Available scenes:", pinned_scene_name);
            for (const auto &wrapper : pl->get_scenes())
                error("  - {}", wrapper->get_name());
            return 1;
        }
    }
#endif // ENABLE_EMULATOR

    string host = "0.0.0.0";
    server_t server{
        restinio::own_io_context(),
        [port, host, is_debugging](auto &settings)
        {
            std::unique_lock lock(Server::registryMutex);
            auto router = Server::server_handler(Server::registry);
            if (is_debugging)
                router->http_get("/exit_debug", [](auto req, auto)
                                 {
                                     interrupt_received = true;
                                     exit_canvas_update = true;

                                     return req->create_response().set_body("Sent interrupt.").done();
                                 });

            settings.port(port);
            settings.address(host);
            settings.request_handler(std::move(router));
            settings.write_http_response_timelimit(30s);
            settings.cleanup_func([]
                                  {
            std::unique_lock lock(Server::registryMutex);
            Server::registry.clear(); });
        }};

    thread control_thread{
        [&server, &port, &host]
        {
            // Use restinio::run to launch RESTinio's server.
            // This run() will return only if server stopped from
            // some other thread.
            info("Listening on http://{}:{}/", host, port);
            run(on_thread_pool(
                1,                                      // Count of worker threads for RESTinio.
                restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                server)                                 // Server to be run.
            );
        }};

    for (const auto &item : pl->get_plugins())
    {
        const auto err = item->after_server_init();
        if (err.has_value())
        {
            error(err.value());
            std::exit(-1);
        }
    }

    debug("Starting UDP server on port {}", port);
    UdpServer *udpServer = new UdpServer(port);

    debug("Initializing hardware...");
#ifdef ENABLE_EMULATOR
    auto hardware_code = start_hardware_mainloop(matrix, pinned_scene);
#else
    auto hardware_code = start_hardware_mainloop(matrix);
#endif

    if (hardware_code != 0)
    {
        error("Could not initialize hardware_code.");
        initiate_shutdown(server);

        info("Terminating plugin loader...");
        pl->destroy_plugins();

        return hardware_code;
    }

    info("Hardware thread stopped. Stopping http server");

    initiate_shutdown(server);

    delete udpServer;

    for (const auto plugin : pl->get_plugins())
    {
        if (auto err = plugin->pre_exit(); err.has_value())
        {
            error(err.value());
        }
    }

    info("Saving config...");
    config->save();

    delete Constants::global_post_processor;
    delete Constants::global_transition_manager;

    info("Joining control thread...");
    control_thread.join();

    pl->delete_references();

    info("Destroying config instance...");
    delete config;

    info("Terminating plugin loader...");
    pl->destroy_plugins();

    info("Stopping UpdateManager...");
    if (Constants::global_update_manager)
    {
        Constants::global_update_manager->stop();
        delete Constants::global_update_manager;
        Constants::global_update_manager = nullptr;
    }


    return 0;
}

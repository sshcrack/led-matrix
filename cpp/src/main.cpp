
#include "spdlog/spdlog.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>


#include "spotify/shared_spotify.h"
#include "spotify/spotify.h"
#include <Magick++.h>
#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"
#include "server/server.h"
#include "shared/utils/shared.h"

using namespace spdlog;
using namespace std;
using json = nlohmann::json;
using Plugins::PluginManager;

int main(int argc, char *argv[]) {
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();

    debug("Loading plugins...");
    auto pl = PluginManager::instance();
    pl->initialize();

    auto scenes = pl->get_scenes();
    auto image_types = pl->get_image_providers();
    info("Loaded {} Scenes and {} Image Types", scenes.size(), image_types.size());

    debug("Loading config...");
    config = new Config::MainConfig("config.json");

    debug("Checking spotify config...");
    spotify = new Spotify();
    spotify->initialize();

    debug("Starting mainloop_thread");
    uint16_t port = 8080;

    string host = "0.0.0.0";

    server_t server{
            restinio::own_io_context(),
            restinio::server_settings_t<>{}
                    .port(port)
                    .address(host)
                    .request_handler(req_handler)
    };

    thread control_thread{[&server, &port, &host] {
        // Use restinio::run to launch RESTinio's server.
        // This run() will return only if server stopped from
        // some other thread.
        info("Listening on http://{}:{}/", host, port);
        restinio::run(restinio::on_thread_pool(
                1, // Count of worker threads for RESTinio.
                restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                server) // Server to be run.
        );
    }
    };

    debug("Initializing...");
    auto hardware = initialize_hardware(argc, argv);

    if (!hardware.has_value()) {
        error("Could not initialize hardware.");
        restinio::initiate_shutdown(server);
        return hardware.error();
    }

    info("Hardware initialized successfully");

    hardware->wait();
    info("Hardware thread stopped. Saving config...");
    config->save();


    info("Stopping http server");

    restinio::initiate_shutdown(server);
    spotify->terminate();
    control_thread.join();

    pl->terminate();
    return 0;
}

#include "spdlog/spdlog.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>


#include "../plugins/SpotifyScenes/manager/shared_spotify.h"
#include "../plugins/SpotifyScenes/manager/spotify.h"
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


    debug("Loading config...");
    config = new Config::MainConfig("config.json");

    for (const auto &item: pl->get_plugins()) {
        auto err = item->post_init();
        if (err.has_value()) {
            spdlog::error(err.value());
            std::exit(-1);
        }
    }

    auto scenes = pl->get_scenes();
    auto image_types = pl->get_image_providers();
    info("Loaded {} Scenes and {} Image Types", scenes.size(), image_types.size());


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
                2, // Count of worker threads for RESTinio.
                restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                server) // Server to be run.
        );
    }
    };

    debug("Initializing hardware...");
    auto hardware_code = 0;//start_hardware_mainloop(argc, argv);

    if (hardware_code != 0) {
        error("Could not initialize hardware_code.");
        restinio::initiate_shutdown(server);

        debug("Terminating plugin loader...");
        pl->terminate();

        return hardware_code;
    }

    info("Hardware thread stopped. Saving config...");
    config->save();


    info("Stopping http server");

    restinio::initiate_shutdown(server);

    debug("Joining control thread...");
    control_thread.join();

    debug("Deleting config...");
    delete config;

    debug("Terminating plugin loader...");
    pl->terminate();

    return 0;
}
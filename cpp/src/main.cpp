
#include "server/server.h"
#include <Magick++.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "shared.h"
#include <nlohmann/json.hpp>
#include "matrix_control/hardware.h"

using namespace spdlog;
using namespace std;
using json = nlohmann::json;

int main(int argc, char *argv[]) {
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();
    debug("Loading config");

    //TODO fix issues for height not being available when fetching images
    config = new Config::MainConfig("config.json");
    config->save();

    debug("Starting mainloop");
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
    info("Saving config...");
    config->save();


    info("Stopping http server");

    restinio::initiate_shutdown(server);
    control_thread.join();
    return 0;
}
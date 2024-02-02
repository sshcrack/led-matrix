
#include "server/server.h"
#include <Magick++.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "shared.h"
#include "matrix_control/hardware.h"

using namespace spdlog;
using namespace std;

int main(int argc, char *argv[]) {
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();
    debug("Loading config");

    config = new Config("config.json");

    debug("Starting mainloop");
    uint16_t port = 8080;

    server_t server{
            restinio::own_io_context(),
            restinio::server_settings_t<>{}
                    .port(port)
                    .address("localhost")
                    .request_handler(req_handler)
    };

    thread control_thread{[&server, &port] {
        // Use restinio::run to launch RESTinio's server.
        // This run() will return only if server stopped from
        // some other thread.
        info("Listening on http://localhost:{}/", port);
        restinio::run(restinio::on_thread_pool(
                1, // Count of worker threads for RESTinio.
                restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                server) // Server to be run.
        );
    }
    };

    debug("Initializing... val: {}", config->get_str("test"));
    auto hardware = initialize_hardware(argc, argv);

    if (!hardware.has_value()) {
        error("Could not initialize hardware.");
        restinio::initiate_shutdown(server);
        return hardware.error();
    }

    info("Hardware initialized successfully");

    hardware->wait();

    info("Stopping http server");

    restinio::initiate_shutdown(server);
    control_thread.join();
    return 0;
}
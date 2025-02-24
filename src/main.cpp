#include "spdlog/spdlog.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>
#include <utility>


#include <Magick++.h>
#include <shared/utils/consts.h>

#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"
#include "server/server.h"
#include "shared/utils/shared.h"

using namespace spdlog;
using namespace std;
using json = nlohmann::json;
using Plugins::PluginManager;

using server_t = restinio::http_server_t<>;
int usage(const char *progname) {
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

int main(int argc, char *argv[]) {
    Magick::InitializeMagick(*argv);
    cfg::load_env_levels();

    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    // Should be in hardware.cpp but this actually drops privileges, so I moved it here

    debug("Parsing rgb matrix from cmdline");
    runtime_opt.drop_priv_user = getenv("SUDO_UID");
    runtime_opt.drop_priv_group = getenv("SUDO_GID");

    if (!ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt)) {
        return usage(argv[0]);
    }

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == nullptr)
        return usage(argv[0]);

    if (!filesystem::exists(Constants::root_dir)) {
        filesystem::create_directory(Constants::root_dir);
    }

    debug("Loading plugins...");
    const auto pl = PluginManager::instance();
    pl->initialize();


    debug("Loading config...");
    config = new Config::MainConfig("config.json");

    for (const auto &item: pl->get_plugins()) {
        const auto err = item->post_init();
        if (err.has_value()) {
            error(err.value());
            std::exit(-1);
        }
    }

    info("Loaded {} Scenes and {} Image Types", pl->get_scenes().size(), pl->get_image_providers().size());


    debug("Starting mainloop_thread");
    uint16_t port = 8080;

    string host = "0.0.0.0";

    server_t server{
        restinio::own_io_context(),
        [port, host](auto & settings) {
            std::shared_ptr router = Server::server_handler();
            
            // Create request handler function that uses the router
            auto handler = [router = std::move(router)]
                (auto req) { return (*router)(std::move(req)); };
                
            settings.port(port);
            settings.address(host);
            settings.request_handler(std::move(handler));
        }
    };

    thread control_thread{
        [&server, &port, &host] {
            // Use restinio::run to launch RESTinio's server.
            // This run() will return only if server stopped from
            // some other thread.
            info("Listening on http://{}:{}/", host, port);
            run(on_thread_pool(
                    2, // Count of worker threads for RESTinio.
                    restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                    server) // Server to be run.
            );
        }
    };

    debug("Initializing hardware...");
    auto hardware_code = start_hardware_mainloop(matrix);

    if (hardware_code != 0) {
        error("Could not initialize hardware_code.");
        initiate_shutdown(server);

        debug("Terminating plugin loader...");
        pl->destroy_plugins();

        return hardware_code;
    }

    info("Hardware thread stopped. Stopping http server");

    initiate_shutdown(server);

    for (const auto plugin: pl->get_plugins()) {
        if (auto err = plugin->pre_exit(); err.has_value()) {
            error(err.value());
        }
    }

    info("Saving config...");
    config->save();

    debug("Joining control thread...");
    control_thread.join();

    pl->delete_references();

    debug("Deleting config...");
    delete config;


    debug("Terminating plugin loader...");
    pl->destroy_plugins();

    return 0;
}

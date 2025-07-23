#include "spdlog/spdlog.h"
#include "shared/matrix/plugin_loader/loader.h"
#include <nlohmann/json.hpp>
#include <utility>

#include <Magick++.h>
#include <shared/matrix/utils/consts.h>

#include "spdlog/cfg/env.h"
#include "matrix_control/hardware.h"
#include "matrix-factory.h"
#include "server/server.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "udp.h"
#include "shared/matrix/server/common.h"

#include <restinio/websocket/websocket.hpp>

using namespace spdlog;
using namespace std;
using json = nlohmann::json;
using Plugins::PluginManager;

using server_t = restinio::http_server_t<Server::traits_t>;

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

    rgb_matrix::MatrixFactory::Options options;

    // Should be in hardware.cpp but this actually drops privileges, so I moved it here

    debug("Parsing rgb matrix from cmdline");
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

    debug("Loading plugins...");
    const auto pl = PluginManager::instance();
    pl->initialize();

    debug("Loading config...");
    config = new Config::MainConfig("config.json");

    for (const auto &item : pl->get_plugins())
    {
        const auto err = item->before_server_init();
        if (err.has_value())
        {
            error(err.value());
            std::exit(-1);
        }
    }

    info("Loaded {} Scenes and {} Image Types", pl->get_scenes().size(), pl->get_image_providers().size());

    debug("Starting mainloop_thread");
    uint16_t port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;

    string host = "0.0.0.0";

#ifdef ENABLE_CORS
    debug("Allowing CORS request to be made to this server");
#endif

    server_t server{
        restinio::own_io_context(),
        [port, host](auto &settings)
        {
            std::shared_ptr router = Server::server_handler(registry);

            // Create request handler function that handles OPTIONS first, then delegates to router
            auto handler = [router = std::move(router)](auto req)
            {
#ifdef ENABLE_CORS
                // Handle CORS preflight requests for all routes
                if (req->header().method() == restinio::http_method_options())
                {
                    return Server::handle_cors_preflight(req);
                }
#endif
                return (*router)(std::move(req));
            };

            settings.port(port);
            settings.address(host);
            settings.request_handler(Server::server_handler(registry));
            settings.read_next_http_message_timelimit(10s);
            settings.write_http_response_timelimit(1s);
            settings.handle_request_timeout(1s);
            settings.cleanup_func([]
                                  { registry.clear(); });
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
    auto hardware_code = start_hardware_mainloop(matrix);

    if (hardware_code != 0)
    {
        error("Could not initialize hardware_code.");
        initiate_shutdown(server);

        debug("Terminating plugin loader...");
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

    debug("Joining control thread...");
    control_thread.join();

    pl->delete_references();

    debug("Deleting config...");
    delete config;

    debug("Terminating plugin loader...");
    pl->destroy_plugins();

    return 0;
}

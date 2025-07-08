#include "server.h"
#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include <nlohmann/json.hpp>

#include "canvas_status.h"
#include "shared/plugin_loader/loader.h"
#include "other_routes.h"
#include "preset_management.h"
#include "scene_management.h"
#include "shared/server/server_utils.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace restinio;
using json = nlohmann::json;


// Create request handler.
std::unique_ptr<router_t> Server::server_handler() {
    auto router = std::make_unique<router_t>();

#ifdef ENABLE_CORS
    // Handle CORS preflight requests for all routes
    router->add_handler(http_method_options(), R"(.*)", [](auto req, auto) {
        return Server::handle_cors_preflight(req);
    });
    spdlog::debug("Allowing CORS requests");
#endif

    router = add_preset_routes(std::move(router));
    router = add_canvas_status_routes(std::move(router));
    router = add_scene_routes(std::move(router));
    router = add_other_routes(std::move(router));

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_plugins()) {
        router = item->register_routes(std::move(router));
    }

    return router;
}
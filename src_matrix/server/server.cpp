#include "server.h"
#include <restinio/all.hpp>
#include "shared/matrix/utils/shared.h"
#include <nlohmann/json.hpp>

#include "canvas_status.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "other_routes.h"
#include "desktop_ws.h"
#include "preset_management.h"
#include "scene_management.h"
#include "post_processing_routes.h"
#include "update_routes.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>
#include "schedule_management.h"

using namespace std;
using namespace restinio;

using json = nlohmann::json;

// External reference to global UpdateManager
extern Update::UpdateManager* global_update_manager;


// Create request handler.
std::unique_ptr<router_t> Server::server_handler(ws_registry_t & registry ) {
    auto router = std::make_unique<router_t>();

    router = add_preset_routes(std::move(router));
    router = add_canvas_status_routes(std::move(router));
    router = add_scene_routes(std::move(router));
    router = add_schedule_routes(std::move(router));
    router = add_post_processing_routes(std::move(router));
    router = add_update_routes(std::move(router), global_update_manager);
    router = add_other_routes(std::move(router));
    router = add_desktop_routes(std::move(router), registry);

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_plugins()) {
        router = item->register_routes(std::move(router));
    }

    return router;
}
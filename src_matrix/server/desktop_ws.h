#pragma once
#include "shared/matrix/server/common.h"

namespace Config {
class MainConfig;
}

namespace Server
{
std::unique_ptr<router_t> add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t& registry, Config::MainConfig* cfg);
void broadcast_matrix_enabled(bool enabled);
}
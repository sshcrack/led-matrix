#pragma once
#include "server.h"

namespace Server {
    /// GET/POST /environment (real-world sync settings, overrides, live
    /// conditions) and GET /environment/geocode?q= (place search).
std::unique_ptr<router_t> add_environment_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg);
}

#pragma once
#include "server.h"

namespace Server {
    /// GET/POST /halo: rear halo light settings and sender status.
    std::unique_ptr<router_t> add_halo_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg);
}

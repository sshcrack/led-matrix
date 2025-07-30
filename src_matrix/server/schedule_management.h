#pragma once
#include "server.h"

namespace Server {
    std::unique_ptr<router_t> add_schedule_routes(std::unique_ptr<router_t> router);
}

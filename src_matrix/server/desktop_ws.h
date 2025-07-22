#pragma once
#include "common.h"

namespace Server
{
    using namespace std;

    std::unique_ptr<router_t> add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t &registry);
}
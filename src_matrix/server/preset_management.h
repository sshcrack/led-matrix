#pragma once
#include "restinio/all.hpp"

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    using namespace std;

    std::unique_ptr<router_t> add_preset_routes(std::unique_ptr<router_t> router);
}
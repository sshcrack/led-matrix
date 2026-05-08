#pragma once
#include "restinio/all.hpp"

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    std::unique_ptr<router_t> add_custom_assets_routes(std::unique_ptr<router_t> router);
}


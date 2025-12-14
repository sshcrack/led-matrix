#pragma once
#include <restinio/core.hpp>

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    std::unique_ptr<router_t> add_plugin_routes(std::unique_ptr<router_t> router);
}

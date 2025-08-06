#pragma once

#include <restinio/all.hpp>
#include <memory>

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    
    std::unique_ptr<router_t> add_marketplace_routes(std::unique_ptr<router_t> router);
}
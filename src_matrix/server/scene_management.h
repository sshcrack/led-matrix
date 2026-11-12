#pragma once
#include "restinio/all.hpp"

class TransitionManager;

namespace Server {
    using router_t = restinio::router::express_router_t<>;

    std::unique_ptr<router_t> add_scene_routes(std::unique_ptr<router_t> router, TransitionManager* transition_manager);
}
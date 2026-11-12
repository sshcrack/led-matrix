#pragma once
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

class PostProcessor;
class TransitionManager;

namespace Server
{
    namespace rws = restinio::websocket::basic;

    using router_t = restinio::router::express_router_t<>;
    using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;

    /// Objects the routes use. Owned by the Daemon, which stops the HTTP server
    /// before destroying them, so routes may hold these pointers.
    struct Dependencies {
        PostProcessor* post_processor = nullptr;
        TransitionManager* transition_manager = nullptr;
    };

    std::unique_ptr<router_t> server_handler(ws_registry_t& registry, const Dependencies& deps);
}
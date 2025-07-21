#pragma once
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

namespace Server
{
    namespace rws = restinio::websocket::basic;

    using router_t = restinio::router::express_router_t<>;
    using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;
    using namespace std;

    std::unique_ptr<router_t> server_handler(ws_registry_t &registry);

}
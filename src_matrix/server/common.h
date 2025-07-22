#pragma once
#include <restinio/all.hpp>
#include <restinio/websocket/websocket.hpp>

namespace Server {
namespace rws = restinio::websocket::basic;
using router_t = restinio::router::express_router_t<>;
using traits_t =
    restinio::traits_t<
        restinio::asio_timer_manager_t,
        restinio::single_threaded_ostream_logger_t,
        router_t>;

// Alias for container with stored websocket handles.
using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;
}
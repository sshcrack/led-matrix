#pragma once
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared_mutex>
#include "shared/matrix/Scene.h"

namespace Server {
    namespace rws = restinio::websocket::basic;
    using router_t = restinio::router::express_router_t<>;
    using traits_t =
        restinio::traits_t<
            restinio::asio_timer_manager_t,
            restinio::null_logger_t,
            router_t>;

    // Alias for container with stored websocket handles.
    using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;

    extern std::shared_mutex registryMutex;
    extern ws_registry_t registry;

    extern std::shared_mutex currSceneMutex;
    extern std::shared_ptr<Scenes::Scene> currScene;
}



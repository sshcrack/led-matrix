#pragma once
#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared_mutex>
#include <atomic>
#include "shared/matrix/Scene.h"

#ifdef _WIN32
#ifdef SHARED_MATRIX_EXPORTS
#define SHARED_MATRIX_API __declspec(dllexport)
#else
#define SHARED_MATRIX_API __declspec(dllimport)
#endif
#else
#define SHARED_MATRIX_API
#endif

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

    extern SHARED_MATRIX_API std::shared_mutex registryMutex;
    extern SHARED_MATRIX_API ws_registry_t registry;
    extern SHARED_MATRIX_API std::atomic<int> desktop_connection_count;

    extern SHARED_MATRIX_API std::shared_mutex currSceneMutex;
    extern SHARED_MATRIX_API std::shared_ptr<Scenes::Scene> currScene;
}


#pragma once
#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared_mutex>
#include <atomic>
#include <cstdint>
#include "shared/matrix/Scene.h"
#include "shared/matrix/export.h"

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

    struct DesktopProducerChange {
        std::uint64_t previous_owner = 0;
        std::uint64_t owner = 0;
        bool changed = false;
    };

    // Desktop controllers may be connected concurrently, but some plugins
    // represent one physical producer stream (SpotifyMV is the first). The
    // newest regular desktop connection owns those producers. This also makes
    // an automatic reconnect supersede a stale server-side socket immediately.
    SHARED_MATRIX_API DesktopProducerChange register_desktop_producer(std::uint64_t connection_id);
    SHARED_MATRIX_API DesktopProducerChange unregister_desktop_producer(std::uint64_t connection_id);
    SHARED_MATRIX_API void clear_desktop_producers();
    SHARED_MATRIX_API std::uint64_t desktop_producer_owner();
    SHARED_MATRIX_API bool accepts_desktop_producer_message(std::uint64_t connection_id);

    extern SHARED_MATRIX_API std::shared_mutex currSceneMutex;
    extern SHARED_MATRIX_API std::shared_ptr<Scenes::Scene> currScene;
}

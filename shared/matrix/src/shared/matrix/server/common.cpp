#include "shared/matrix/server/common.h"

#include <mutex>
#include <set>


namespace Server {
    std::shared_mutex registryMutex;
    ws_registry_t registry;
    std::atomic<int> desktop_connection_count{0};

    namespace {
        std::mutex desktop_producer_mutex;
        std::set<std::uint64_t> desktop_producers;
        std::atomic<std::uint64_t> desktop_producer_owner_id{0};
    }

    DesktopProducerChange register_desktop_producer(const std::uint64_t connection_id) {
        std::lock_guard lock(desktop_producer_mutex);
        const auto previous = desktop_producer_owner_id.load(std::memory_order_relaxed);
        desktop_producers.insert(connection_id);
        // Newest live controller wins. In particular, a reconnect must replace
        // a stale socket that the server has not observed closing yet.
        desktop_producer_owner_id.store(connection_id, std::memory_order_release);
        return {previous, connection_id, previous != connection_id};
    }

    DesktopProducerChange unregister_desktop_producer(const std::uint64_t connection_id) {
        std::lock_guard lock(desktop_producer_mutex);
        const auto previous = desktop_producer_owner_id.load(std::memory_order_relaxed);
        desktop_producers.erase(connection_id);
        auto owner = previous;
        if (previous == connection_id) {
            owner = desktop_producers.empty() ? 0 : *desktop_producers.rbegin();
            desktop_producer_owner_id.store(owner, std::memory_order_release);
        }
        return {previous, owner, previous != owner};
    }

    void clear_desktop_producers() {
        std::lock_guard lock(desktop_producer_mutex);
        desktop_producers.clear();
        desktop_producer_owner_id.store(0, std::memory_order_release);
    }

    std::uint64_t desktop_producer_owner() {
        return desktop_producer_owner_id.load(std::memory_order_acquire);
    }

    bool accepts_desktop_producer_message(const std::uint64_t connection_id) {
        return connection_id != 0 && desktop_producer_owner() == connection_id;
    }

    std::shared_mutex currSceneMutex;
    std::shared_ptr<Scenes::Scene> currScene;
}
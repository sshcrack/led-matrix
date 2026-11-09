#include "shared/matrix/server/common.h"

#include <chrono>
#include <mutex>
#include <map>
#include <string>


namespace Server {
    std::shared_mutex registryMutex;
    ws_registry_t registry;
    std::atomic<int> desktop_connection_count{0};

    namespace {
        std::mutex desktop_producer_mutex;
        std::map<std::uint64_t, std::string> desktop_producers;
        std::map<std::uint64_t, std::string> desktop_workers;
        std::atomic<std::uint64_t> desktop_producer_owner_id{0};

        struct Relevance { int score = 0; std::int64_t reported_ms = 0; };
        std::map<std::uint64_t, Relevance> desktop_relevance;
        // The standby desktop currently ahead of the owner, and since when.
        std::uint64_t challenger_id = 0;
        std::int64_t challenger_since_ms = 0;

        std::int64_t steady_ms() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        int fresh_score(std::uint64_t id, std::int64_t now_ms) {
            const auto it = desktop_relevance.find(id);
            if (it == desktop_relevance.end() || now_ms - it->second.reported_ms > DesktopRelevanceStaleMs)
                return 0;
            return it->second.score;
        }

        // Most relevant remaining desktop; ties go to the newest connection.
        std::uint64_t best_candidate(std::uint64_t exclude, std::int64_t now_ms) {
            std::uint64_t best = 0;
            int best_score = -1;
            for (const auto &[id, client] : desktop_producers) {
                if (id == exclude) continue;
                const int score = fresh_score(id, now_ms);
                if (score >= best_score) { best = id; best_score = score; }
            }
            return best;
        }
    }

    DesktopProducerChange register_desktop_producer(const std::uint64_t connection_id, std::string client_id) {
        std::lock_guard lock(desktop_producer_mutex);
        const auto previous = desktop_producer_owner_id.load(std::memory_order_relaxed);
        desktop_producers[connection_id] = std::move(client_id);

        auto owner = previous;
        if (previous == 0) {
            owner = connection_id;
        } else {
            const auto previous_it = desktop_producers.find(previous);
            const auto current_it = desktop_producers.find(connection_id);
            const bool same_logical_client = previous_it != desktop_producers.end()
                && current_it != desktop_producers.end()
                && !current_it->second.empty()
                && previous_it->second == current_it->second;
            if (same_logical_client) {
                // Replace only our own stale transport. Removing the superseded
                // connection from the candidate set prevents it being promoted
                // again if the fresh socket closes before TCP teardown is seen.
                desktop_producers.erase(previous_it);
                desktop_relevance.erase(previous);
                challenger_id = 0;
                owner = connection_id;
            }
        }

        desktop_producer_owner_id.store(owner, std::memory_order_release);
        return {previous, owner, previous != owner};
    }

    DesktopProducerChange unregister_desktop_producer(const std::uint64_t connection_id) {
        std::lock_guard lock(desktop_producer_mutex);
        const auto previous = desktop_producer_owner_id.load(std::memory_order_relaxed);
        desktop_producers.erase(connection_id);
        desktop_relevance.erase(connection_id);
        challenger_id = 0;
        auto owner = previous;
        if (previous == connection_id) {
            owner = best_candidate(0, steady_ms());
            desktop_producer_owner_id.store(owner, std::memory_order_release);
        }
        return {previous, owner, previous != owner};
    }

    DesktopProducerChange report_desktop_relevance(const std::uint64_t connection_id, const int score,
                                                   std::int64_t now_ms) {
        if (now_ms < 0) now_ms = steady_ms();
        std::lock_guard lock(desktop_producer_mutex);
        const auto previous = desktop_producer_owner_id.load(std::memory_order_relaxed);
        if (!desktop_producers.contains(connection_id))
            return {previous, previous, false};
        // A challenger that went silent counted as 0 meanwhile: its hold restarts.
        if (challenger_id == connection_id && fresh_score(connection_id, now_ms) == 0
            && now_ms - desktop_relevance[connection_id].reported_ms > DesktopRelevanceStaleMs)
            challenger_id = 0;
        desktop_relevance[connection_id] = {score, now_ms};
        if (previous == 0)
            return {previous, previous, false};

        const int owner_score = fresh_score(previous, now_ms);
        const auto candidate = best_candidate(previous, now_ms);
        const bool ahead = candidate != 0 && fresh_score(candidate, now_ms) >= owner_score + DesktopRelevanceMargin;
        if (!ahead) {
            challenger_id = 0;
            return {previous, previous, false};
        }
        if (challenger_id != candidate) {
            challenger_id = candidate;
            challenger_since_ms = now_ms;
            return {previous, previous, false};
        }
        if (now_ms - challenger_since_ms < DesktopRelevanceHoldMs)
            return {previous, previous, false};

        challenger_id = 0;
        desktop_producer_owner_id.store(candidate, std::memory_order_release);
        return {previous, candidate, true};
    }

    void register_desktop_worker(const std::uint64_t connection_id, std::string client_id) {
        std::lock_guard lock(desktop_producer_mutex);
        desktop_workers[connection_id] = std::move(client_id);
    }

    void unregister_desktop_worker(const std::uint64_t connection_id) {
        std::lock_guard lock(desktop_producer_mutex);
        desktop_workers.erase(connection_id);
    }

    void clear_desktop_producers() {
        std::lock_guard lock(desktop_producer_mutex);
        desktop_producers.clear();
        desktop_workers.clear();
        desktop_relevance.clear();
        challenger_id = 0;
        desktop_producer_owner_id.store(0, std::memory_order_release);
    }

    std::uint64_t desktop_producer_owner() {
        return desktop_producer_owner_id.load(std::memory_order_acquire);
    }

    bool accepts_desktop_producer_message(const std::uint64_t connection_id) {
        if (connection_id == 0)
            return false;
        std::lock_guard lock(desktop_producer_mutex);
        const auto owner = desktop_producer_owner_id.load(std::memory_order_relaxed);
        if (owner == connection_id)
            return true;
        const auto owner_it = desktop_producers.find(owner);
        const auto worker_it = desktop_workers.find(connection_id);
        if (owner_it == desktop_producers.end() || worker_it == desktop_workers.end())
            return false;
        if (!owner_it->second.empty())
            return owner_it->second == worker_it->second;
        // Backward compatibility for one legacy desktop/worker pair that does
        // not yet send client_id. Ambiguous multi-client legacy setups stay
        // fail-closed rather than recreating the producer race.
        return desktop_producers.size() == 1 && desktop_workers.size() == 1
            && worker_it->second.empty();
    }

    std::vector<std::uint64_t> desktop_producer_targets() {
        std::lock_guard lock(desktop_producer_mutex);
        std::vector<std::uint64_t> targets;
        const auto owner = desktop_producer_owner_id.load(std::memory_order_relaxed);
        if (owner == 0)
            return targets;
        targets.push_back(owner);
        const auto owner_it = desktop_producers.find(owner);
        if (owner_it == desktop_producers.end())
            return targets;
        if (owner_it->second.empty()) {
            if (desktop_producers.size() == 1 && desktop_workers.size() == 1
                && desktop_workers.begin()->second.empty())
                targets.push_back(desktop_workers.begin()->first);
            return targets;
        }
        for (const auto &[connection_id, client_id] : desktop_workers)
            if (client_id == owner_it->second) targets.push_back(connection_id);
        return targets;
    }

    std::shared_mutex currSceneMutex;
    std::shared_ptr<Scenes::Scene> currScene;
}
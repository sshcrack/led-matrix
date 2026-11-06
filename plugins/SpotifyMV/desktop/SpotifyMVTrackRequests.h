#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

/// Track-request state of the SpotifyMV desktop producer.
///
/// Two slots coexist: `current` (the engine sending frames) and `pending`
/// (a replacement that is searching/decoding in the background). Each slot
/// carries the generation of the request that created it; engine callbacks
/// and the search thread capture that generation and act only while it is
/// still the right one.
///
///   request(T) with T == current, no pending      → AlreadyCurrent
///   request(T) with T == pending                  → AlreadyPending
///   request(T) with T == current, other pending   → ReturnToCurrent (pending cancelled)
///   request(T) otherwise                          → NewRequest (pending = {T, new gen})
///   promote(gen, T) while pending == {T, gen}     → current = pending, pending cleared
///   clear()                                       → both slots empty (stop/standby)
///
/// Not internally locked: the owner serializes calls (SpotifyMVDesktop uses
/// track_id_mutex_). Only `status_generation()` is safe to read without the
/// lock, so engine status callbacks never need the plugin mutexes.
class SpotifyMVTrackRequests {
public:
    struct Slot {
        std::string track_id;
        std::uint64_t generation = 0;
    };

    enum class Outcome { AlreadyCurrent, AlreadyPending, ReturnToCurrent, NewRequest };
    struct Decision {
        Outcome outcome = Outcome::AlreadyCurrent;
        std::uint64_t generation = 0; ///< New pending generation for NewRequest.
    };

    Decision request(const std::string &track_id)
    {
        if (has(pending_) && pending_.track_id == track_id)
            return {Outcome::AlreadyPending};
        if (has(current_) && current_.track_id == track_id) {
            if (!has(pending_)) return {Outcome::AlreadyCurrent};
            pending_ = {};
            ++generation_;
            publish();
            return {Outcome::ReturnToCurrent};
        }
        pending_ = Slot{track_id, ++generation_};
        publish();
        return {Outcome::NewRequest, pending_.generation};
    }

    /// True while `generation` is the pending request, i.e. its search and
    /// decoder should keep going.
    [[nodiscard]] bool is_pending(std::uint64_t generation) const
    {
        return has(pending_) && pending_.generation == generation;
    }

    [[nodiscard]] bool is_pending(std::uint64_t generation, const std::string &track_id) const
    {
        return is_pending(generation) && pending_.track_id == track_id;
    }

    /// Pending → current. Returns false for a superseded callback.
    bool promote(std::uint64_t generation, const std::string &track_id)
    {
        if (!is_pending(generation, track_id)) return false;
        current_ = std::move(pending_);
        pending_ = {};
        publish();
        return true;
    }

    /// Stop/standby/destruction: forget both slots and invalidate every
    /// outstanding callback.
    void clear()
    {
        current_ = {};
        pending_ = {};
        ++generation_;
        publish();
    }

    /// Generation whose engine status should reach the matrix: the pending
    /// request while one is loading, otherwise the current track. 0 = none.
    /// Lock-free so engine threads can check it from any context.
    [[nodiscard]] std::uint64_t status_generation() const
    {
        return status_generation_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::optional<Slot> current() const { return has(current_) ? std::optional(current_) : std::nullopt; }
    [[nodiscard]] std::optional<Slot> pending() const { return has(pending_) ? std::optional(pending_) : std::nullopt; }

private:
    void publish()
    {
        status_generation_.store(has(pending_) ? pending_.generation : current_.generation,
                                 std::memory_order_release);
    }

    /// Generations start at 1, so generation 0 marks an empty slot.
    static bool has(const Slot &slot) { return slot.generation != 0; }

    Slot current_;
    Slot pending_;
    std::uint64_t generation_ = 0;
    std::atomic<std::uint64_t> status_generation_{0};
};

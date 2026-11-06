#pragma once

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "shared/matrix/Scene.h"
#include "shared/matrix/runtime_inputs.h"

/// Scene Lab owns at most one temporary session.
///
/// State machine (all transitions under `mutex_`):
///
///   Inactive --start--> Active(session id S, lease)
///   Active(S) --start--> Active(S') (replacement; S is now stale)
///   Active(S) --update(S, expected generation)--> Active(S) with generation+1
///   Active(S) --heartbeat(S)--> Active(S) with renewed lease
///   Active(S) --stop(S) | lease elapsed--> Inactive
///
/// `generation_` advances on every transition and never resets, so any
/// holder of an older generation (the canvas, a queued browser update) can
/// detect that it has been superseded. An Active session whose lease has
/// elapsed is treated as Inactive everywhere: mutating calls end it first,
/// read-only calls report it inactive.
class SceneLabRuntime {
public:
    using Clock = std::chrono::steady_clock;

    /// The caller named a session that is no longer the active one (replaced,
    /// stopped, or expired). The HTTP layer maps this to 409 Conflict so the
    /// browser drops its ownership.
    struct StaleSession : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct Snapshot {
        bool active = false;
        std::uint64_t session_id = 0;
        std::uint64_t generation = 0;
        std::shared_ptr<Scenes::Scene> scene;
        std::string scene_name;
        std::string variant;
        nlohmann::json properties = nlohmann::json::object();
        int fps = 20;
        std::vector<std::string> missing_inputs;
    };

    static SceneLabRuntime &instance();

    Snapshot start(const std::string &scene_name,
                   const std::string &variant,
                   const nlohmann::json &properties,
                   int fps,
                   const RuntimeInputs::Snapshot &runtime_inputs);
    Snapshot update(const std::string &variant,
                    const nlohmann::json &properties,
                    int fps,
                    const RuntimeInputs::Snapshot &runtime_inputs,
                    std::optional<std::uint64_t> expected_generation = std::nullopt,
                    std::optional<std::uint64_t> expected_session_id = std::nullopt);
    /// Ends the session. With an expected id, stopping a different active
    /// session throws StaleSession; stopping when nothing is active is a no-op.
    void stop(std::optional<std::uint64_t> expected_session_id = std::nullopt);
    void heartbeat(std::optional<std::uint64_t> expected_session_id = std::nullopt);
    [[nodiscard]] bool lease_active(std::uint64_t generation) const;
    [[nodiscard]] Snapshot snapshot(const RuntimeInputs::Snapshot &runtime_inputs = {}) const;
    [[nodiscard]] nlohmann::json status_json(const RuntimeInputs::Snapshot &runtime_inputs = {}) const;

    /// Tests only: replace the lease clock.
    void set_clock_for_testing(std::function<Clock::time_point()> clock);
    static constexpr auto LeaseDuration = std::chrono::seconds(45);

private:
    struct Session {
        std::uint64_t id = 0;
        std::shared_ptr<Scenes::Scene> scene;
        std::string scene_name;
        std::string variant;
        nlohmann::json properties = nlohmann::json::object();
        int fps = 20;
        Clock::time_point lease_expires_at{};
    };

    SceneLabRuntime() = default;
    static std::shared_ptr<Scenes::Scene> build_scene(
        const std::string &scene_name, const std::string &variant,
        const nlohmann::json &properties, int fps);

    [[nodiscard]] Clock::time_point now() const;
    /// Active session whose lease has not elapsed, or nullptr.
    [[nodiscard]] const Session *live_session_locked(Clock::time_point now) const;
    /// Ends a session whose lease has elapsed so mutating calls see Inactive.
    void expire_locked(Clock::time_point now);
    /// Throws StaleSession unless `expected` names the live session.
    Session &require_session_locked(std::optional<std::uint64_t> expected, Clock::time_point now);
    void end_session_locked();
    [[nodiscard]] Snapshot snapshot_locked(Clock::time_point now) const;

    mutable std::mutex mutex_;
    std::optional<Session> session_;
    std::uint64_t generation_ = 0;
    std::uint64_t next_session_id_ = 1;
    std::function<Clock::time_point()> clock_;
};

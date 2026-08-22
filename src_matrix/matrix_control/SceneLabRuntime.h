#pragma once

#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "shared/matrix/Scene.h"
#include "shared/matrix/runtime_inputs.h"

class SceneLabRuntime {
public:
    struct Snapshot {
        bool active = false;
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
                    const RuntimeInputs::Snapshot &runtime_inputs);
    void stop();
    void heartbeat();
    [[nodiscard]] bool lease_active(std::uint64_t generation) const;
    [[nodiscard]] Snapshot snapshot(const RuntimeInputs::Snapshot &runtime_inputs = {}) const;
    [[nodiscard]] nlohmann::json status_json(const RuntimeInputs::Snapshot &runtime_inputs = {}) const;

private:
    SceneLabRuntime() = default;
    static std::shared_ptr<Scenes::Scene> build_scene(
        const std::string &scene_name, const std::string &variant,
        const nlohmann::json &properties, int fps);

    mutable std::mutex mutex_;
    Snapshot state_;
    std::chrono::steady_clock::time_point lease_expires_at_{};
    static constexpr auto LeaseDuration = std::chrono::seconds(45);
};

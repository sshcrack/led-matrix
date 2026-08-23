#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <shared/matrix/scene_runtime.h>

namespace RuntimeInputs {

using SignalValue = std::variant<bool, std::int64_t, double, std::string>;
using Signals = std::unordered_map<std::string, SignalValue>;

struct InputState {
    bool available = false;
    bool stale = false;
    double age_seconds = 0.0;
    std::optional<double> ttl_seconds;
    Signals signals;
};

class Snapshot {
public:
    Snapshot() = default;
    explicit Snapshot(std::unordered_map<std::string, InputState> inputs)
        : inputs_(std::move(inputs)) {}

    [[nodiscard]] bool available(std::string_view id) const;
    [[nodiscard]] const InputState *find(std::string_view id) const;
    [[nodiscard]] std::optional<SignalValue> signal(
        std::string_view id, std::string_view name) const;
    [[nodiscard]] std::optional<double> number(
        std::string_view id, std::string_view name) const;
    [[nodiscard]] std::optional<bool> boolean(
        std::string_view id, std::string_view name) const;
    [[nodiscard]] std::optional<std::string> text(
        std::string_view id, std::string_view name) const;

    [[nodiscard]] const std::unordered_map<std::string, InputState> &all() const {
        return inputs_;
    }

private:
    std::unordered_map<std::string, InputState> inputs_;
};

/// Publish an available Runtime Input. A finite TTL makes availability expire
/// automatically when producers stop refreshing it; no background timer is used.
void publish(std::string_view id, Signals signals = {},
             std::optional<std::chrono::milliseconds> ttl = std::nullopt);

/// Explicitly mark a Runtime Input available or unavailable. Sticky inputs such
/// as desktop connectivity normally omit the TTL.
void set_available(std::string_view id, bool available, Signals signals = {},
                   std::optional<std::chrono::milliseconds> ttl = std::nullopt);

/// Remove one Runtime Input entirely. Prefer set_available(false) when the
/// distinction between "known but unavailable" and "never published" matters.
void clear(std::string_view id);

/// Test/preview cleanup helper.
void clear_all();

[[nodiscard]] Snapshot snapshot();
[[nodiscard]] std::vector<std::string> missing_required(
    const Scenes::SceneInputSpec &spec, const Snapshot &snapshot);
[[nodiscard]] bool satisfies(
    const Scenes::SceneInputSpec &spec, const Snapshot &snapshot);
[[nodiscard]] nlohmann::json to_json(const Snapshot &snapshot);

/// Replace this process' generic input mirror from a serialized snapshot. Used
/// by the desktop scene worker so scene code reads the same RuntimeInputs API
/// regardless of where it is executing.
void replace_from_json(const nlohmann::json &snapshot_json);

} // namespace RuntimeInputs

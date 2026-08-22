#include "SceneLabRuntime.h"

#include <algorithm>
#include <stdexcept>

#include "shared/matrix/plugin_loader/loader.h"

SceneLabRuntime &SceneLabRuntime::instance()
{
    static SceneLabRuntime runtime;
    return runtime;
}

std::shared_ptr<Scenes::Scene> SceneLabRuntime::build_scene(
    const std::string &scene_name, const std::string &variant,
    const nlohmann::json &properties, int fps)
{
    if (!properties.is_object()) throw std::invalid_argument("Scene Lab properties must be an object");
    for (const auto &wrapper : Plugins::PluginManager::instance()->get_scenes()) {
        if (!wrapper || wrapper->get_name() != scene_name) continue;
        auto scene = wrapper->create();
        if (!scene) throw std::runtime_error("Scene factory returned null");
        scene->update_default_properties();
        scene->register_properties();

        if (properties.is_object()) {
            const auto registered_properties = scene->get_properties();
            for (auto it = properties.begin(); it != properties.end(); ++it) {
                const bool known = std::any_of(registered_properties.begin(), registered_properties.end(),
                    [&](const auto &property) { return property && property->getName() == it.key(); });
                if (!known)
                    throw std::invalid_argument("Unknown property '" + it.key() + "' for scene '" + scene_name + "'");
            }
        }

        if (!variant.empty()) {
            const auto descriptor = scene->get_descriptor();
            if (Scenes::find_variant(descriptor, variant)) scene->apply_variant(variant);
            else scene->set_external_variant_id(variant);
        }
        scene->load_properties(properties);
        scene->set_runtime_target_fps(std::clamp(fps, 10, 30));
        return std::shared_ptr<Scenes::Scene>(std::move(scene));
    }
    throw std::invalid_argument("Unknown scene '" + scene_name + "'");
}

SceneLabRuntime::Snapshot SceneLabRuntime::start(
    const std::string &scene_name, const std::string &variant,
    const nlohmann::json &properties, int fps,
    const RuntimeInputs::Snapshot &runtime_inputs)
{
    auto scene = build_scene(scene_name, variant, properties, fps);
    Snapshot next;
    next.active = true;
    next.scene = std::move(scene);
    next.scene_name = scene_name;
    next.variant = variant;
    next.properties = next.scene->to_json();
    next.fps = std::clamp(fps, 10, 30);
    next.missing_inputs = RuntimeInputs::missing_required(next.scene->get_effective_runtime_inputs(), runtime_inputs);

    std::lock_guard lock(mutex_);
    next.generation = state_.generation + 1;
    state_ = next;
    lease_expires_at_ = std::chrono::steady_clock::now() + LeaseDuration;
    return state_;
}

SceneLabRuntime::Snapshot SceneLabRuntime::update(
    const std::string &variant, const nlohmann::json &properties, int fps,
    const RuntimeInputs::Snapshot &runtime_inputs)
{
    std::string scene_name;
    {
        std::lock_guard lock(mutex_);
        if (!state_.active) throw std::runtime_error("Scene Lab is not active");
        scene_name = state_.scene_name;
    }
    return start(scene_name, variant, properties, fps, runtime_inputs);
}

void SceneLabRuntime::stop()
{
    std::lock_guard lock(mutex_);
    const auto generation = state_.generation + 1;
    state_ = {};
    state_.generation = generation;
    lease_expires_at_ = {};
}

void SceneLabRuntime::heartbeat()
{
    std::lock_guard lock(mutex_);
    if (!state_.active) return;
    const auto now = std::chrono::steady_clock::now();
    if (now >= lease_expires_at_) {
        const auto generation = state_.generation + 1;
        state_ = {};
        state_.generation = generation;
        lease_expires_at_ = {};
        return;
    }
    lease_expires_at_ = now + LeaseDuration;
}

bool SceneLabRuntime::lease_active(std::uint64_t generation) const
{
    std::lock_guard lock(mutex_);
    return state_.active && state_.generation == generation
        && std::chrono::steady_clock::now() < lease_expires_at_;
}

SceneLabRuntime::Snapshot SceneLabRuntime::snapshot(const RuntimeInputs::Snapshot &runtime_inputs) const
{
    std::lock_guard lock(mutex_);
    auto result = state_;
    if (result.active && std::chrono::steady_clock::now() >= lease_expires_at_) {
        result.active = false; result.scene.reset(); result.scene_name.clear(); result.variant.clear();
    }
    if (result.active && result.scene)
        result.missing_inputs = RuntimeInputs::missing_required(result.scene->get_effective_runtime_inputs(), runtime_inputs);
    return result;
}

nlohmann::json SceneLabRuntime::status_json(const RuntimeInputs::Snapshot &runtime_inputs) const
{
    const auto value = snapshot(runtime_inputs);
    return {
        {"active", value.active}, {"generation", value.generation},
        {"scene", value.scene_name}, {"variant", value.variant},
        {"properties", value.properties}, {"fps", value.fps},
        {"missing_inputs", value.missing_inputs}
    };
}

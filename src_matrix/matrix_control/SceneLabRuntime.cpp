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

SceneLabRuntime::Clock::time_point SceneLabRuntime::now() const
{
    return clock_ ? clock_() : Clock::now();
}

void SceneLabRuntime::set_clock_for_testing(std::function<Clock::time_point()> clock)
{
    std::lock_guard lock(mutex_);
    clock_ = std::move(clock);
}

const SceneLabRuntime::Session *SceneLabRuntime::live_session_locked(const Clock::time_point now) const
{
    if (!session_ || now >= session_->lease_expires_at) return nullptr;
    return &*session_;
}

void SceneLabRuntime::end_session_locked()
{
    session_.reset();
    ++generation_;
}

void SceneLabRuntime::expire_locked(const Clock::time_point now)
{
    if (session_ && !live_session_locked(now)) end_session_locked();
}

SceneLabRuntime::Session &SceneLabRuntime::require_session_locked(
    const std::optional<std::uint64_t> expected, const Clock::time_point now)
{
    expire_locked(now);
    if (!session_) {
        if (expected.has_value()) throw StaleSession("Scene Lab session is stale: it has ended or expired");
        throw std::runtime_error("Scene Lab is not active");
    }
    if (expected.has_value() && *expected != session_->id)
        throw StaleSession("Scene Lab session is stale: another session replaced it");
    return *session_;
}

SceneLabRuntime::Snapshot SceneLabRuntime::snapshot_locked(const Clock::time_point now) const
{
    Snapshot result;
    result.generation = generation_;
    if (const auto *session = live_session_locked(now)) {
        result.active = true;
        result.session_id = session->id;
        result.scene = session->scene;
        result.scene_name = session->scene_name;
        result.variant = session->variant;
        result.properties = session->properties;
        result.fps = session->fps;
    }
    return result;
}

SceneLabRuntime::Snapshot SceneLabRuntime::start(
    const std::string &scene_name, const std::string &variant,
    const nlohmann::json &properties, int fps,
    const RuntimeInputs::Snapshot &runtime_inputs)
{
    Session next;
    next.scene = build_scene(scene_name, variant, properties, fps);
    next.scene_name = scene_name;
    next.variant = variant;
    next.properties = next.scene->to_json();
    next.fps = std::clamp(fps, 10, 30);
    auto missing = RuntimeInputs::missing_required(next.scene->get_effective_runtime_inputs(), runtime_inputs);

    std::lock_guard lock(mutex_);
    const auto t = now();
    next.id = next_session_id_++;
    if (next_session_id_ == 0) next_session_id_ = 1;
    next.lease_expires_at = t + LeaseDuration;
    session_ = std::move(next);
    ++generation_;
    auto result = snapshot_locked(t);
    result.missing_inputs = std::move(missing);
    return result;
}

SceneLabRuntime::Snapshot SceneLabRuntime::update(
    const std::string &variant, const nlohmann::json &properties, int fps,
    const RuntimeInputs::Snapshot &runtime_inputs,
    std::optional<std::uint64_t> expected_generation,
    std::optional<std::uint64_t> expected_session_id)
{
    // Building a scene is slow, so validate, build unlocked, then re-validate
    // that nothing moved in between before committing.
    std::string scene_name;
    std::uint64_t base_generation = 0;
    std::uint64_t base_session_id = 0;
    {
        std::lock_guard lock(mutex_);
        const auto &session = require_session_locked(expected_session_id, now());
        if (expected_generation.has_value() && *expected_generation != generation_)
            throw StaleSession("Scene Lab update is stale");
        scene_name = session.scene_name;
        base_generation = generation_;
        base_session_id = session.id;
    }

    Session next;
    next.scene = build_scene(scene_name, variant, properties, fps);
    next.id = base_session_id;
    next.scene_name = scene_name;
    next.variant = variant;
    next.properties = next.scene->to_json();
    next.fps = std::clamp(fps, 10, 30);
    auto missing = RuntimeInputs::missing_required(next.scene->get_effective_runtime_inputs(), runtime_inputs);

    std::lock_guard lock(mutex_);
    const auto t = now();
    require_session_locked(base_session_id, t);
    if (generation_ != base_generation)
        throw StaleSession("Scene Lab update is stale");
    next.lease_expires_at = t + LeaseDuration;
    session_ = std::move(next);
    ++generation_;
    auto result = snapshot_locked(t);
    result.missing_inputs = std::move(missing);
    return result;
}

void SceneLabRuntime::stop(std::optional<std::uint64_t> expected_session_id)
{
    std::lock_guard lock(mutex_);
    expire_locked(now());
    if (!session_) return;
    if (expected_session_id.has_value() && *expected_session_id != session_->id)
        throw StaleSession("Scene Lab session is stale: another session replaced it");
    end_session_locked();
}

void SceneLabRuntime::heartbeat(std::optional<std::uint64_t> expected_session_id)
{
    std::lock_guard lock(mutex_);
    const auto t = now();
    if (!expected_session_id.has_value()) {
        expire_locked(t);
        if (session_) session_->lease_expires_at = t + LeaseDuration;
        return;
    }
    require_session_locked(expected_session_id, t).lease_expires_at = t + LeaseDuration;
}

bool SceneLabRuntime::lease_active(std::uint64_t generation) const
{
    std::lock_guard lock(mutex_);
    return generation_ == generation && live_session_locked(now()) != nullptr;
}

SceneLabRuntime::Snapshot SceneLabRuntime::snapshot(const RuntimeInputs::Snapshot &runtime_inputs) const
{
    std::lock_guard lock(mutex_);
    auto result = snapshot_locked(now());
    if (result.active && result.scene)
        result.missing_inputs = RuntimeInputs::missing_required(result.scene->get_effective_runtime_inputs(), runtime_inputs);
    return result;
}

nlohmann::json SceneLabRuntime::status_json(const RuntimeInputs::Snapshot &runtime_inputs) const
{
    const auto value = snapshot(runtime_inputs);
    return {
        {"active", value.active}, {"session_id", value.session_id}, {"generation", value.generation},
        {"scene", value.scene_name}, {"variant", value.variant},
        {"properties", value.properties}, {"fps", value.fps},
        {"missing_inputs", value.missing_inputs}
    };
}

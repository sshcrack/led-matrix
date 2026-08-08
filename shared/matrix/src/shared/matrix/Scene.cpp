#include "shared/matrix/Scene.h"

#include <shared/common/utils/utils.h>
#include <shared/matrix/utils/utils.h>
#include <shared/matrix/utils/uuid.h>
#include "spdlog/spdlog.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/plugin/property.h"
#include "shared/matrix/FallbackScene.h"
#include <algorithm>
#include <chrono>

using namespace spdlog;

std::unique_ptr<Scenes::Scene> Scenes::Scene::from_json(const nlohmann::json &j)
{
    if (!j.contains("type"))
        throw std::runtime_error(fmt::format("No scene type given for '{}'", j.dump()));

    const string t = j["type"].get<string>();
    const nlohmann::json &arguments = j.value("arguments", nlohmann::json::object());

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item : pl->get_scenes())
    {
        if (item->get_name() != t)
            continue;

        auto scene = item->create();

        spdlog::debug("Creating scene '{}'", t);
        scene->update_default_properties();
        scene->register_properties();

        spdlog::debug("Loading properties for scene '{}'", t);
        scene->load_properties(arguments);
        if (j.contains("uuid"))
            scene->uuid = j["uuid"].get<string>();

        if (scene->uuid.empty())
            scene->uuid = uuid::generate_uuid_v4();
        return scene;
    }

    spdlog::warn("Unknown scene type '{}', returning FallbackScene", t);
    auto unknown_scene = std::make_unique<Scenes::FallbackScene>(t);

    unknown_scene->arguments = arguments;
    if (j.contains("uuid"))
        unknown_scene->uuid = j["uuid"].get<string>();

    if (unknown_scene->uuid.empty())
        unknown_scene->uuid = uuid::generate_uuid_v4();

    return unknown_scene;
}

void Scenes::Scene::initialize(int width, int height)
{
    if (initialized)
        return;

    matrix_width = width;
    matrix_height = height;
    initialized = true;
    reset_frame_clock();
}

bool Scenes::Scene::is_initialized() const
{
    return initialized;
}

nlohmann::json Scenes::Scene::to_json() const
{
    nlohmann::json j;
    for (const auto &item : properties)
    {
        item->dump_to_json(j);
    }

    return j;
}

tmillis_t Scenes::Scene::get_duration() const
{
    return duration->get();
}

tmillis_t Scenes::Scene::get_transition_duration() const
{
    return transition_duration->get();
}

std::string Scenes::Scene::get_transition_name() const
{
    return transition_name->get();
}

int Scenes::Scene::get_weight() const
{
    return weight->get();
}

void Scenes::Scene::wait_until_next_frame()
{
    if (suppress_internal_wait_)
        return;

    const tmillis_t step = std::max<tmillis_t>(1, 1000 / std::max(1, target_fps));
    const tmillis_t current_time = GetTimeInMillis();
    if (last_render_time == 0 || last_render_time + step <= current_time)
    {
        last_render_time = current_time;
        return;
    }

    const tmillis_t deadline = last_render_time + step;
    SleepMillis(deadline - current_time);
    last_render_time = deadline;
}


void Scenes::Scene::reset_frame_clock()
{
    frame_context_ = {};
    frame_clock_started_ = false;
    last_render_time = 0;
}

bool Scenes::Scene::render_frame(FrameCanvas *canvas,
                                 std::optional<double> forced_delta_seconds,
                                 bool suppress_internal_wait)
{
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();

    double delta = 0.0;
    bool deterministic = forced_delta_seconds.has_value();
    if (forced_delta_seconds.has_value()) {
        delta = std::clamp(*forced_delta_seconds, 0.0, 0.25);
        if (!frame_clock_started_) {
            frame_clock_start_ = now;
            frame_clock_last_ = now;
            frame_clock_started_ = true;
        }
    } else if (!frame_clock_started_) {
        frame_clock_start_ = now;
        frame_clock_last_ = now;
        frame_clock_started_ = true;
        delta = 1.0 / static_cast<double>(std::max(1, target_fps));
    } else {
        delta = std::clamp(std::chrono::duration<double>(now - frame_clock_last_).count(), 0.0, 0.25);
        frame_clock_last_ = now;
    }

    frame_context_.delta_seconds = delta;
    frame_context_.elapsed_seconds += delta;
    frame_context_.frame_index += 1;
    frame_context_.now_ms = static_cast<std::uint64_t>(frame_context_.elapsed_seconds * 1000.0);
    frame_context_.deterministic = deterministic;

    const bool previous_suppress = suppress_internal_wait_;
    suppress_internal_wait_ = suppress_internal_wait || deterministic;
    try {
        const bool result = render(canvas);
        suppress_internal_wait_ = previous_suppress;
        return result;
    } catch (...) {
        suppress_internal_wait_ = previous_suppress;
        throw;
    }
}

Scenes::Scene::Scene()
{
    weight->label("Playlist weight").description("Relative probability of this scene being selected from a preset.")
        .group("Playback").step(1).control("number").advanced();
    duration->label("Scene duration").description("How long this scene remains active before the playlist advances.")
        .group("Playback").unit("duration").control("duration")
        .presets(nlohmann::json::array({5000, 15000, 30000, 60000, 120000}));
    transition_duration->label("Transition duration").description("Blend time into the next scene. Use 0 for an immediate switch.")
        .group("Transition").unit("duration").control("duration")
        .presets(nlohmann::json::array({0, 150, 250, 500, 750, 1000, 2000}));
    transition_name->label("Transition style").description("Transition effect used when this scene ends.")
        .group("Transition").control("select");

    add_property(weight);
    add_property(duration);
    add_property(transition_duration);
    add_property(transition_name);
}

void Scenes::Scene::after_render_stop()
{
}

void Scenes::Scene::before_transition_stop()
{
}

void Scenes::Scene::load_properties(const json &j)
{
    for (const auto &item : properties)
    {
        item->load_from_json(j);
    }
}

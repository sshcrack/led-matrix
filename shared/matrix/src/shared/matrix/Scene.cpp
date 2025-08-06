#include "shared/matrix/Scene.h"

#include <shared/common/utils/utils.h>
#include <shared/matrix/utils/utils.h>
#include <shared/matrix/utils/uuid.h>
#include "spdlog/spdlog.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/plugin/property.h"
#include "shared/matrix/FallbackScene.h"

using namespace spdlog;

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> Scenes::Scene::from_json(const nlohmann::json &j)
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

        scene->update_default_properties();
        scene->register_properties();
        scene->load_properties(arguments);
        if (j.contains("uuid"))
            scene->uuid = j["uuid"].get<string>();

        if (scene->uuid.empty())
            scene->uuid = uuid::generate_uuid_v4();
        return scene;
    }

    spdlog::warn("Unknown scene type '{}', returning FallbackScene", t);
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> unknown_scene = {
        new Scenes::FallbackScene(t), [](Scenes::Scene *scene)
        {
            delete scene;
        }};

    ((FallbackScene*) unknown_scene.get())->arguments = arguments;
    if (j.contains("uuid"))
        unknown_scene->uuid = j["uuid"].get<string>();

    if (unknown_scene->uuid.empty())
        unknown_scene->uuid = uuid::generate_uuid_v4();

    return unknown_scene;
}

void Scenes::Scene::initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas)
{
    if (initialized)
        return;

    offscreen_canvas = l_offscreen_canvas;
    matrix_height = matrix->height();
    matrix_width = matrix->width();
    initialized = true;

    // Call update_default_properties() here after the scene is initialized
    update_default_properties();
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

int Scenes::Scene::get_weight() const
{
    return weight->get();
}

void Scenes::Scene::wait_until_next_frame()
{
    tmillis_t step = 1000 / target_fps;
    tmillis_t current_time = GetTimeInMillis();

    if (last_render_time + step < current_time)
    {
        last_render_time = current_time;
        return;
    }

    SleepMillis(last_render_time + step - current_time);
    last_render_time = current_time;
}

Scenes::Scene::Scene()
{
    add_property(weight);
    add_property(duration);
}

void Scenes::Scene::after_render_stop(RGBMatrixBase *matrix)
{
}

void Scenes::Scene::load_properties(const json &j)
{
    for (const auto &item : properties)
    {
        item->load_from_json(j);
    }
}

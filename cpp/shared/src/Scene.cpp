#include "Scene.h"

#include <spdlog/spdlog.h>
#include "shared/plugin_loader/loader.h"
#include "plugin/property.h"

using namespace spdlog;

Scenes::Scene *Scenes::Scene::from_json(const nlohmann::json &j) {
    if (!j.contains("type"))
        throw std::runtime_error(fmt::format("No scene type given for '{}'", j.dump()));

    string t = j["type"].get<string>();
    const nlohmann::json &arguments = j.value("arguments", nlohmann::json::object());

    auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_scenes()) {
        if (item->get_name() == t) {
            Scene *scene = item->create();

            scene->register_properties();
            scene->load_properties(arguments);
            return scene;
        }
    }

    throw std::runtime_error(fmt::format("Invalid type '{}'", t));
}

void Scenes::Scene::initialize(rgb_matrix::RGBMatrix *matrix) {
    if (initialized)
        return;

    if (create_offscreen)
        offscreen_canvas = matrix->CreateFrameCanvas();

    matrix_height = matrix->height();
    matrix_width = matrix->width();
    initialized = true;
}

bool Scenes::Scene::is_initialized() const {
    return initialized;
}

nlohmann::json Scenes::Scene::to_json() const {
    nlohmann::json j;
    for (auto item: properties) {
        item.second->dump_to_json(j);

    }

    return j;
}

tmillis_t Scenes::Scene::get_duration() const {
    return duration.get();
}

int Scenes::Scene::get_weight() const {
    return weight.get();
}

Scenes::Scene::Scene(bool p_create_offscreen) {
    create_offscreen = p_create_offscreen;

    add_property(&weight, &duration);
}

void Scenes::Scene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {}

void Scenes::Scene::load_properties(const json &j) {
    for (const auto &item: properties) {
        item.second->load_from_json(j);
    }
}

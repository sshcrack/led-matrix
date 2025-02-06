#include "Scene.h"

#include <spdlog/spdlog.h>
#include "shared/plugin_loader/loader.h"

using namespace spdlog;

Scenes::Scene *Scenes::Scene::from_json(const nlohmann::json &j) {
    if (!j.contains("type"))
        throw std::runtime_error(fmt::format("No scene type given for '{}'", j.dump()));

    string t = j["type"].get<string>();
    const nlohmann::json &arguments = j.value("arguments", nlohmann::json::object());

    auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_scenes()) {
        if (item->get_name() == t) {
            return item->from_json(arguments);
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
    return {
            {"weight",   weight},
            {"duration", duration}
    };
}

tmillis_t Scenes::Scene::get_duration() const {
    return duration;
}

int Scenes::Scene::get_weight() const {
    return weight;
}

Scenes::Scene::Scene(const json &json, bool p_create_offscreen) {
    if (!json.contains("weight") || !json.contains("duration"))
        throw std::runtime_error("Scene json does not contain weight or duration");
    weight = json["weight"];
    duration = json["duration"];
    create_offscreen = p_create_offscreen;
}

nlohmann::json Scenes::Scene::create_default(int weight, tmillis_t duration) {
    return {
            {"weight",   weight},
            {"duration", duration}
    };
}

void Scenes::Scene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {}

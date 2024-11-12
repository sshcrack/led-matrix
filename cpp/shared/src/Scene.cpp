#include "Scene.h"

#include <spdlog/spdlog.h>
#include <utility>
#include "shared/plugin_loader/loader.h"

using namespace spdlog;

Scenes::Scene *Scenes::Scene::from_json(const nlohmann::json &j) {
    string t = j["type"].get<string>();
    const nlohmann::json &arguments = j["arguments"];

    auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_scenes()) {
        if (item->get_name() == t) {
            return item->from_json(arguments);
        }
    }

    throw std::runtime_error(fmt::format("Invalid type '{}'", t));
}

void Scenes::Scene::initialize(RGBMatrix *matrix) {
    if (initialized)
        return;

    offscreen_canvas = matrix->CreateFrameCanvas();
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

Scenes::Scene::Scene(const json &json) {
    weight = json["weight"];
    duration = json["duration"];
}

nlohmann::json Scenes::Scene::create_default(int weight, tmillis_t duration) {
    return {
            {"weight",   weight},
            {"duration", duration}
    };
}

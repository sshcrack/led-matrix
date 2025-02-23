#include "shared/config/image_providers/general.h"
#include "shared/plugin_loader/loader.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include "fmt/core.h"


ImageProviders::General::General(const json& arguments) {
    spdlog::debug("Initial arguments for general are {}", arguments.dump());
}

ImageProviders::General::~General() = default;

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> ImageProviders::General::from_json(const json &j) {
    if(!j.contains("type"))
        throw std::runtime_error(fmt::format("No image provider type given for '{}'", j.dump()));

    const string t = j["type"].get<string>();
    const json& arguments = j.value("arguments", json::object());

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_image_providers()) {
        if(item->get_name() == t) {
            return item->from_json(arguments);
        }
    }

    throw std::runtime_error(fmt::format("Invalid image provider type '{}'", t));
}

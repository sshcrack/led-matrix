#include "shared/config/image_providers/general.h"
#include "shared/plugin_loader/loader.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include "fmt/core.h"


ImageProviders::General::General(const json& arguments) {
    spdlog::debug("Initial arguments for general are {}", arguments.dump());
}

ImageProviders::General* ImageProviders::General::from_json(const json &j) {
    string t = j["type"].get<string>();
    const json& arguments = j["arguments"];

    auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_image_providers()) {
        if(item->get_name() == t) {
            return item->from_json(arguments);
        }
    }

    throw std::runtime_error(fmt::format("Invalid image provider type '{}'", t));
}

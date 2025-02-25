#include "shared/config/image_providers/general.h"
#include "shared/plugin_loader/loader.h"
#include "spdlog/spdlog.h"
#include <stdexcept>
#include <shared/utils/uuid.h>

#include "fmt/core.h"


ImageProviders::General::General() {
}

ImageProviders::General::~General() = default;

void ImageProviders::General::load_properties(const nlohmann::json &j) {
    for (const auto &item: properties) {
        item->load_from_json(j);
    }
}

nlohmann::json ImageProviders::General::to_json() const {
    nlohmann::json j;
    for (const auto& item: properties) {
        item->dump_to_json(j);
    }

    return j;
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> ImageProviders::General::from_json(const json &j) {
            spdlog::debug("From json serialization");
    if(!j.contains("type"))
        throw std::runtime_error(fmt::format("No image provider type given for '{}'", j.dump()));

    const string t = j["type"].get<string>();
    const json& arguments = j.value("arguments", json::object());

    const bool has_uuid = j.contains("uuid") && j["uuid"].is_string();

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_image_providers()) {
        if(item->get_name() == t) {
            auto provider = item->create();

            spdlog::debug("Registering and loading");
            provider->register_properties();
            provider->load_properties(arguments);
            if (has_uuid)
                provider->uuid = j["uuid"].get<string>();
            else
                provider->uuid = uuid::generate_uuid_v4();

            return std::move(provider);  // Use std::move to transfer ownership
        }
    }

    throw std::runtime_error(fmt::format("Invalid image provider type '{}'", t));
}

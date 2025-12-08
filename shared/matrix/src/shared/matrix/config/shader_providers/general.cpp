#include "spdlog/spdlog.h"
#include <stdexcept>
#include <shared/matrix/config/shader_providers/general.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/utils/uuid.h>


ShaderProviders::General::General() = default;

ShaderProviders::General::~General() = default;

void ShaderProviders::General::load_properties(const nlohmann::json &j) {
    for (const auto &item: properties) {
        item->load_from_json(j);
    }
}

nlohmann::json ShaderProviders::General::to_json() const {
    json j;
    j["type"] = get_name();
    j["uuid"] = uuid;

    json arguments;
    for (const auto &item: properties) {
        item->dump_to_json(arguments);
    }

    j["arguments"] = arguments;
    return j;
}

std::unique_ptr<ShaderProviders::General, void (*)(ShaderProviders::General *)> ShaderProviders::General::from_json(const json &j) {
    spdlog::debug("From json serialization for shader provider");
    if(!j.contains("type"))
        throw std::runtime_error(fmt::format("No shader provider type given for '{}'", j.dump()));

    const std::string t = j["type"].get<std::string>();
    const json& arguments = j.value("arguments", json::object());

    const bool has_uuid = j.contains("uuid") && j["uuid"].is_string();

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_shader_providers()) {
        if(item->get_name() == t) {
            auto provider = item->create();

            spdlog::debug("Registering and loading shader provider");
            provider->update_default_properties();
            provider->register_properties();
            provider->load_properties(arguments);
            if (has_uuid)
                provider->uuid = j["uuid"].get<string>();
            else
                provider->uuid = uuid::generate_uuid_v4();

            return std::move(provider);
        }
    }

    throw std::runtime_error(fmt::format("Invalid shader provider type '{}'", t));
}

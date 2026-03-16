#include "scene_management.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/canvas_consts.h"

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_scene_routes(std::unique_ptr<router_t> router) {
    // GET routes
    router->http_get("/get_curr", [](auto req, auto) {
        std::vector<json> scenes;

        for (const auto &item: config->get_curr()->scenes) {
            json j;
            j["name"] = item->get_name();
            j["properties"] = item->to_json();

            scenes.push_back(j);
        }

        return reply_with_json(req, scenes);
    });

    router->http_get("/list_scenes", [](auto req, auto) {
        auto scenes = Plugins::PluginManager::instance()->get_scenes();
        std::vector<json> j;

        for (const auto &item: scenes) {
            auto properties = item->get_default()->get_properties();
            std::vector<json> properties_json;

            for (const auto &item1: properties) {
                json j1;
                item1->dump_to_json(j1);

                json additional_data;
                item1->add_additional_data(additional_data);

                if (item1->getName() == "transition_name") {
                    additional_data["enum_name"] = "Transition";
                    additional_data["enum_values"] = json::array();
                    additional_data["empty_value_sentinel"] = "__GLOBAL_DEFAULT__";

                    // Empty string keeps per-scene transition unset so the preset/global default is used.
                    additional_data["enum_values"].push_back({
                        {"value", "__GLOBAL_DEFAULT__"},
                        {"display_name", "Global Default"}
                    });

                    if (Constants::global_transition_manager) {
                        for (const auto &transition_name: Constants::global_transition_manager->get_registered_transitions()) {
                            additional_data["enum_values"].push_back({
                                {"value", transition_name},
                                {"display_name", transition_name}
                            });
                        }
                    }

                    if (additional_data["enum_values"].size() == 1) {
                        additional_data["enum_values"].push_back({
                            {"value", "blend"},
                            {"display_name", "blend"}
                        });
                    }
                }
                
                json j2;

                j2["additional"] = additional_data;
                j2["name"] = item1->getName();
                j2["default_value"] = j1[item1->getName()];
                if (item1->getName() == "transition_name") {
                    j2["type_id"] = "enum";
                } else {
                    j2["type_id"] = item1->get_type_id();
                }

                properties_json.push_back(j2);
            }

            json j1 = {
                {"name", item->get_name()},
                {"properties", properties_json}
            };

            j.push_back(j1);
        }

        return reply_with_json(req, j);
    });

    router->http_get("/list_providers", [](auto req, auto) {
        auto scenes = Plugins::PluginManager::instance()->get_image_providers();
        std::vector<json> j;

        for (const auto &item: scenes) {
            auto properties = item->get_default()->get_properties();
            std::vector<json> properties_json;

            for (const auto &item1: properties) {
                json j1;
                item1->dump_to_json(j1);

                json j2;
                j2["name"] = item1->getName();
                j2["default_value"] = j1[item1->getName()];
                j2["type_id"] = item1->get_type_id();

                properties_json.push_back(j2);
            }

            json j1 = {
                {"name", item->get_name()},
                {"properties", properties_json}
            };

            j.push_back(j1);
        }

        return reply_with_json(req, j);
    });

    router->http_get("/list_shader_providers", [](auto req, auto) {
        auto providers = Plugins::PluginManager::instance()->get_shader_providers();
        std::vector<json> j;

        for (const auto &item: providers) {
            auto properties = item->get_default()->get_properties();
            std::vector<json> properties_json;

            for (const auto &item1: properties) {
                json j1;
                item1->dump_to_json(j1);

                json j2;
                j2["name"] = item1->getName();
                j2["default_value"] = j1[item1->getName()];
                j2["type_id"] = item1->get_type_id();

                properties_json.push_back(j2);
            }

            json j1 = {
                {"name", item->get_name()},
                {"properties", properties_json}
            };

            j.push_back(j1);
        }

        return reply_with_json(req, j);
    });

    router->http_get("/list_transitions", [](auto req, auto) {
        std::vector<std::string> names;
        if (Constants::global_transition_manager) {
            names = Constants::global_transition_manager->get_registered_transitions();
        }
        nlohmann::json j = names;
        return reply_with_json(req, j);
    });

    return std::move(router);
}

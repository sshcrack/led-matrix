#include "scene_management.h"
#include "shared/utils/shared.h"
#include "shared/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/plugin_loader/loader.h"

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

    return std::move(router);
}

#include "scene_management.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/canvas_consts.h"
#include <filesystem>

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

                json j2;

                j2["additional"] = additional_data;
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

    router->http_get("/scene_preview/:scene_name", [](auto req, auto params) {
        const auto scene_name = std::string(params["scene_name"]);
        const auto exec_dir = get_exec_dir();
        const filesystem::path preview_dir = exec_dir / "previews";
        const filesystem::path file_path = preview_dir / (scene_name + ".png");

        const auto canonical_preview = filesystem::weakly_canonical(preview_dir);
        std::error_code ec;
        const auto canonical_file = filesystem::weakly_canonical(file_path, ec);

        if (ec || !canonical_file.string().starts_with(canonical_preview.string())) {
            return reply_with_error(req, "Invalid scene name", restinio::status_bad_request());
        }

        if (!filesystem::exists(file_path)) {
            return reply_with_error(req, "Preview not found", restinio::status_not_found());
        }

        auto response = req->create_response(restinio::status_ok())
            .append_header_date_field()
            .append_header(restinio::http_field::content_type, "image/png")
            .append_header(restinio::http_field::cache_control, "public, max-age=86400");
        Server::add_cors_headers(response);
        return response.set_body(restinio::sendfile(file_path)).done();
    });

    return std::move(router);
}

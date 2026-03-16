#include "scene_management.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/server/MimeTypes.h"
#include "shared/common/utils/utils.h"
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
                {"properties", properties_json},
                {"has_preview", std::filesystem::exists(get_exec_dir() / "previews" / (item->get_name() + ".gif"))},
                {"needs_desktop", item->get_default()->needs_desktop_app()}
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

    router->http_get("/scene_preview", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("name")) {
            return reply_with_error(req, "No name given");
        }

        const std::string scene_name{qp["name"]};
        const std::filesystem::path preview_dir = get_exec_dir() / "previews";
        const std::filesystem::path gif_path = preview_dir / (scene_name + ".gif");

        // Validate path is inside previews dir
        std::error_code ec;
        if (!std::filesystem::exists(gif_path, ec) || ec) {
            return reply_with_error(req, "Preview not found", restinio::status_not_found());
        }

        const auto canonical_preview_dir = std::filesystem::canonical(preview_dir, ec);
        const auto canonical_gif = std::filesystem::canonical(gif_path, ec);
        if (ec || !canonical_gif.string().starts_with(canonical_preview_dir.string())) {
            return reply_with_error(req, "Invalid path", restinio::status_forbidden());
        }

        auto response = req->create_response(restinio::status_ok())
            .append_header_date_field()
            .append_header(restinio::http_field::content_type, "image/gif");
        Server::add_cors_headers(response);
        return response.set_body(restinio::sendfile(gif_path)).done();
    });

    return std::move(router);
}

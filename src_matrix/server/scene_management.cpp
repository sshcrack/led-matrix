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

namespace {
    std::vector<json> serialize_properties(const std::vector<std::shared_ptr<Plugins::PropertyBase>>& properties, bool include_additional) {
        std::vector<json> properties_json;
        properties_json.reserve(properties.size());
        for (const auto& prop : properties) {
            json j1;
            prop->dump_to_json(j1);
            json j2;
            j2["name"] = prop->getName();
            j2["default_value"] = j1[prop->getName()];
            j2["type_id"] = prop->get_type_id();
            if (include_additional) {
                json additional_data;
                prop->add_additional_data(additional_data);
                j2["additional"] = std::move(additional_data);
            }
            properties_json.push_back(std::move(j2));
        }
        return properties_json;
    }

    template<typename T>
    json make_providers_list(const std::vector<std::shared_ptr<T>>& items) {
        std::vector<json> j;
        j.reserve(items.size());
        for (const auto& item : items) {
            auto properties = item->get_default()->get_properties();
            j.push_back({
                {"name", item->get_name()},
                {"properties", serialize_properties(properties, false)}
            });
        }
        return j;
    }

    json make_scenes_list(const std::vector<std::shared_ptr<Plugins::SceneWrapper>>& scenes) {
        std::vector<json> j;
        j.reserve(scenes.size());
        for (const auto& scene : scenes) {
            auto properties = scene->get_default()->get_properties();
            auto properties_json = serialize_properties(properties, true);
            j.push_back({
                {"name", scene->get_name()},
                {"properties", std::move(properties_json)},
                {"has_preview", std::filesystem::exists(get_exec_dir() / "scene_previews" / (scene->get_name() + ".gif"))},
                {"needs_desktop", scene->get_default()->needs_desktop_app()},
                {"category", scene->get_default()->get_category()}
            });
        }
        return j;
    }
}

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
        return reply_with_json(req, make_scenes_list(Plugins::PluginManager::instance()->get_scenes()));
    });

    router->http_get("/list_providers", [](auto req, auto) {
        return reply_with_json(req, make_providers_list(Plugins::PluginManager::instance()->get_image_providers()));
    });

    router->http_get("/list_shader_providers", [](auto req, auto) {
        return reply_with_json(req, make_providers_list(Plugins::PluginManager::instance()->get_shader_providers()));
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
        const std::filesystem::path preview_dir = get_exec_dir() / "scene_previews";
        const std::filesystem::path gif_path = preview_dir / (scene_name + ".gif");

        // Validate path is inside scene_previews dir
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

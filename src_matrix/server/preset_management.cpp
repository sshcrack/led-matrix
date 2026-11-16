#include "preset_management.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/uuid.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>
#include <shared/matrix/ambiance_packs.h>

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_preset_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg)
{
    router->http_get("/set_active", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "No Id given");
        }

        const string id{qp["id"]};
        const auto presets = cfg->get_presets();
        if (presets.find(id) == presets.end()) {
            return reply_with_error(req, "Invalid id");
        }

        cfg->set_curr(id);
        cfg->set_operation_mode("manual");
        cfg->save();
        PlaybackControl::reload_preset(id);
        return reply_success(req);
    });

    router->http_get("/presets", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        auto presets = cfg->get_presets();
        if (!qp.has("id")) {
            vector<string> keys;
            for (const auto &key: presets | std::views::keys) {
                keys.push_back(key);
            }

            return reply_with_json(req, json(keys));
        }

        const string id{qp["id"]};
        const auto p = presets.find(id);
        if (p == presets.end()) {
            return reply_with_error(req, "Could not find id");
        }

        return reply_with_json(req, json(p->second));
    });

    router->http_get("/list_presets", [cfg](auto req, auto) { return reply_with_json(req, json(cfg->get_presets())); });

    router->http_get("/ambiance_packs", [](auto req, auto) {
        return reply_with_json(req, Ambiance::catalog());
    });
    router->http_post("/ambiance_packs", [cfg](auto req, auto) {
        const auto query = restinio::parse_query(req->header().query());
        if (!query.has("id"))
            return reply_with_error(req, "Pack id is required");
        try {
            auto preset = Ambiance::create_preset(std::string(query["id"]));
            std::string id;
            do {
                id = uuid::generate_uuid_v4();
            } while (cfg->get_presets().contains(id));
            cfg->set_presets(id, preset);
            if (!cfg->save()) {
                cfg->delete_preset(id);
                return reply_with_error(req, "Could not save the preset", restinio::status_internal_server_error());
            }
            return reply_with_json(req, {{"id", id}, {"display_name", preset->display_name}});
        }
        catch (const std::exception& error) {
            return reply_with_error(req, error.what());
        }
    });

    // POST routes
    router->http_post("/add_preset", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        spdlog::debug("Adding preset...");
        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
        }
        catch (std::exception& ex) {
            spdlog::warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset>>();

            std::string id;
            if (qp.has("id")) {
                id = qp["id"];
            }
            else {
                do {
                    id = uuid::generate_uuid_v4();
                } while (cfg->get_presets().contains(id));
            }

            if (id.empty()) {
                return reply_with_error(req, "Id empty");
            }

            if (pr->display_name.empty()) {
                pr->display_name = j.value("display_name", id);
            }

            cfg->set_presets(id, pr);
            cfg->save();
            // Only the preset on screen needs a restart; other edits are just saved.
            if (id == cfg->get_curr_id())
                PlaybackControl::reload_preset(id);
            return reply_with_json(req, {
                {"success", "Preset has been added"},
                {"id", id},
                {"display_name", pr->display_name}
            });
        }
        catch (std::exception& ex) {
            spdlog::warn("Invalid preset with {}", ex.what());
            return reply_with_error(req, "Could not serialize json");
        }
    });

    router->http_post("/preset", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }

        std::string id{qp["id"]};
        string str_body = req->body();
        json j;

        try {
            j = json::parse(str_body);
        }
        catch (std::exception& ex) {
            spdlog::warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset>>();

            if (pr->display_name.empty()) {
                const auto presets = cfg->get_presets();
                const auto existing = presets.find(id);
                if (existing != presets.end() && existing->second) {
                    pr->display_name = existing->second->display_name;
                }
            }

            cfg->set_presets(id, pr);
            cfg->save();
            // Only the preset on screen needs a restart; other edits are just saved.
            if (id == cfg->get_curr_id())
                PlaybackControl::reload_preset(id);
            return reply_with_json(req, {
                {"success", "Preset has been set"},
                {"id", id}
            });
        }
        catch (std::exception& ex) {
            spdlog::warn("Invalid preset with {}", ex.what());
            return reply_with_error(req, "Could not serialize json");
        }
    });

    router->http_post("/preset_display_name", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }

        const std::string id{qp["id"]};
        if (id.empty()) {
            return reply_with_error(req, "Id empty");
        }

        json j;
        try {
            j = json::parse(req->body());
        }
        catch (std::exception& ex) {
            spdlog::warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        if (!j.contains("display_name") || !j["display_name"].is_string()) {
            return reply_with_error(req, "display_name not given");
        }

        const std::string display_name = j["display_name"].get<std::string>();
        if (display_name.empty()) {
            return reply_with_error(req, "display_name empty");
        }

        if (!cfg->set_preset_display_name(id, display_name)) {
            return reply_with_error(req, "Preset not found");
        }

        cfg->save();
        return reply_with_json(req, {
            {"success", "Preset display name updated"},
            {"id", id},
            {"display_name", display_name}
        });
    });

    // DELETE routes
    router->http_delete("/preset", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }

        const std::string id{qp["id"]};
        if (cfg->get_curr_id() == id) {
            return reply_with_error(req, "Can not delete current preset");
        }

        cfg->delete_preset(id);
        return reply_with_json(req, {{"success", "Preset has been deleted"}});
    });

    return router;
}

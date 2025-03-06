#include "preset_management.h"
#include "shared/utils/shared.h"
#include "shared/server/server_utils.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_preset_routes(std::unique_ptr<router_t> router) {
    // GET routes
    router->http_get("/set_preset", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "No Id given");
        }

        const string id{qp["id"]};
        const auto presets = config->get_presets();
        if (presets.find(id) == presets.end()) {
            return reply_with_error(req, "Invalid id");
        }

        config->set_curr(id);
        return reply_success(req);
    });

    router->http_get("/presets", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        auto presets = config->get_presets();
        if (!qp.has("id")) {
            vector<string> keys;
            for (const auto &key: presets | views::keys) {
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

    router->http_get("/list_presets", [](auto req, auto) {
        return reply_with_json(req, json(config->get_presets()));
    });

    // POST routes
    router->http_post("/add_preset", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }
        const std::string id{qp["id"]};
        if (id == "") {
            return reply_with_error(req, "Id empty");
        }

        spdlog::debug("Adding preset...");
        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
        } catch (exception &ex) {
            spdlog::warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset>>();
            config->set_presets(id, pr);
            return reply_with_json(req, {{"success", "Preset has been added"}});
        } catch (exception &ex) {
            spdlog::warn("Invalid preset with {}", ex.what());
            return reply_with_error(req, "Could not serialize json");
        }
    });

    router->http_post("/preset", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }

        std::string id{qp["id"]};
        string str_body = req->body();
        json j;

        try {
            j = json::parse(str_body);
        } catch (exception &ex) {
            spdlog::warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset>>();
            config->set_presets(id, pr);
            config->save();
            return reply_with_json(req, {
                {"success", "Preset has been set"},
                {"id", id}
            });
        } catch (exception &ex) {
            spdlog::warn("Invalid preset with {}", ex.what());
            return reply_with_error(req, "Could not serialize json");
        }
    });

    // DELETE routes
    router->http_delete("/preset", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "Id not given");
        }

        const std::string id{qp["id"]};
        if (config->get_curr_id() == id) {
            return reply_with_error(req, "Can not delete current preset");
        }

        config->delete_preset(id);
        return reply_with_json(req, {{"success", "Preset has been deleted"}});
    });

    return std::move(router);
}

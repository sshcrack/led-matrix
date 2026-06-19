#include "canvas_status.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>


std::unique_ptr<Server::router_t> Server::add_canvas_status_routes(std::unique_ptr<router_t> router) {
    router->http_get("/skip", [](auto req, auto) {
        skip_image.store(true);

        return reply_success(req);
    });

    router->http_get("/toggle", [](auto req, auto) {
        config->set_turned_off(!config->is_turned_off());
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", config->is_turned_off()}});
    });

    router->http_get("/set_enabled", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        std::string enabled_str = qp.has("enabled") ? std::string{qp["enabled"]} : "";
        if (enabled_str.empty()) {
            // Fallback: try JSON body
            try {
                auto body_json = nlohmann::json::parse(req->body());
                if (body_json.contains("enabled")) {
                    enabled_str = body_json["enabled"].is_boolean()
                        ? (body_json["enabled"].get<bool>() ? "true" : "false")
                        : body_json["enabled"].get<std::string>();
                }
            } catch (...) {}
        }
        if (enabled_str.empty()) {
            return reply_with_error(req, "No enabled given");
        }

        config->set_turned_off(enabled_str == "false");
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", config->is_turned_off()}});
    });

    router->http_get("/status", [](auto req, auto) {
        return reply_with_json(req, {
                                   {"turned_off", config->is_turned_off()},
                                   {"turned_on", !config->is_turned_off()},
                                   {"current", config->get_curr_id()}
                               }
        );
    });

    return std::move(router);
}

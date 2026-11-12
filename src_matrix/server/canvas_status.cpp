#include "canvas_status.h"
#include "desktop_ws.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

std::unique_ptr<Server::router_t> Server::add_canvas_status_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg)
{
    router->http_get("/skip", [](auto req, auto) {
        skip_image.store(true);

        return reply_success(req);
    });

    router->http_get("/toggle", [cfg](auto req, auto) {
        cfg->set_turned_off(!cfg->is_turned_off());
        Server::broadcast_matrix_enabled(!cfg->is_turned_off());
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()}});
    });

    router->http_get("/set_enabled", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        std::string enabled_str = qp.has("enabled") ? std::string{qp["enabled"]} : "";
        if (enabled_str.empty()) {
            // Fallback: try JSON body
            try {
                auto body_json = nlohmann::json::parse(req->body());
                if (body_json.contains("enabled")) {
                    if (body_json["enabled"].is_boolean()) {
                        bool bval = body_json["enabled"].template get<bool>();
                        enabled_str = bval ? "true" : "false";
                    }
                    else {
                        enabled_str = body_json["enabled"].template get<std::string>();
                    }
                }
            }
            catch (...) {
            }
        }
        if (enabled_str.empty()) {
            return reply_with_error(req, "No enabled given");
        }

        cfg->set_turned_off(enabled_str == "false");
        Server::broadcast_matrix_enabled(!cfg->is_turned_off());
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()}});
    });

    router->http_get("/operation_mode", [cfg](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("mode"))
            return reply_with_error(req, "No mode given");
        const std::string mode{qp["mode"]};
        if (mode != "automatic" && mode != "manual")
            return reply_with_error(req, "Mode must be automatic or manual");
        cfg->set_operation_mode(mode);
        cfg->save();
        exit_canvas_update.store(true);
        return reply_with_json(req, {{"operation_mode", cfg->get_operation_mode()}});
    });

    router->http_get("/status", [cfg](auto req, auto) {
        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()},
                                     {"turned_on", !cfg->is_turned_off()},
                                     {"current", cfg->get_curr_id()},
                                     {"operation_mode", cfg->get_operation_mode()},
                                     {"director_mode", cfg->get_automatic_director_mode()},
                                     {"automatic_active", cfg->is_automatic_mode()}});
    });

    return router;
}

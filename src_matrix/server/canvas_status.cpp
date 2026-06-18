#include "canvas_status.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>


std::unique_ptr<Server::router_t> Server::add_canvas_status_routes(std::unique_ptr<router_t> router) {
    router->http_post("/skip", [](auto req, auto) {
        skip_image.store(true);

        return reply_success(req);
    });

    router->http_post("/toggle", [](auto req, auto) {
        config->set_turned_off(!config->is_turned_off());
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", config->is_turned_off()}});
    });

    router->http_post("/set_enabled", [](auto req, auto) {
        const auto body = restinio::parse_query(req->body());
        if (!body.has("enabled")) {
            return reply_with_error(req, "No enabled given");
        }

        config->set_turned_off(body["enabled"] == "false");
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

#include "canvas_status.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>


std::unique_ptr<Server::router_t> Server::add_canvas_status_routes(std::unique_ptr<router_t> router) {
    router->http_get("/skip", [](auto req, auto) {
        skip_image.store(true);

        return reply_success(req);
    });

    router->http_get("/toggle", [](auto req, auto) {
        turned_off.store(!turned_off.load());
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", turned_off.load()}});
    });

    router->http_get("/set_enabled", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("enabled")) {
            return reply_with_error(req, "No enabled given");
        }

        turned_off.store(qp["enabled"] == "false");
        exit_canvas_update.store(true);

        return reply_with_json(req, {{"turned_off", turned_off.load()}});
    });

    router->http_get("/status", [](auto req, auto) {
        return reply_with_json(req, {
                                   {"turned_off", turned_off.load()},
                                   {"current", config->get_curr_id()}
                               }
        );
    });

    return std::move(router);
}

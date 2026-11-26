#include "canvas_status.h"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include "desktop_ws.h"
#include "shared/common/Version.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"

std::unique_ptr<Server::router_t> Server::add_canvas_status_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg)
{
    // Routes that change state take POST; GET answers 405 (docs/API.md).
    router->http_get("/skip", [](auto req, auto) { return reply_use_post(req); });
    router->http_post("/skip", [](auto req, auto) {
        skip_image.store(true);
        return reply_success(req);
    });

    router->http_get("/toggle", [](auto req, auto) { return reply_use_post(req); });
    router->http_post("/toggle", [cfg](auto req, auto) {
        const bool saved = cfg->set_turned_off(!cfg->is_turned_off());
        Server::broadcast_matrix_enabled(!cfg->is_turned_off());
        PlaybackControl::restart("power_toggle");
        if (!saved)
            return reply_with_error(req, "Could not save the power state", restinio::status_internal_server_error());
        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()}});
    });

    // enabled=true|false: query, JSON body or form body (the desktop app sends a form).
    router->http_get("/set_enabled", [](auto req, auto) { return reply_use_post(req); });
    router->http_post("/set_enabled", [cfg](auto req, auto) {
        const auto enabled = request_param(req, "enabled");
        if (!enabled || (*enabled != "true" && *enabled != "false"))
            return reply_with_error(req, "enabled must be true or false");
        const bool saved = cfg->set_turned_off(*enabled == "false");
        Server::broadcast_matrix_enabled(!cfg->is_turned_off());
        PlaybackControl::restart("power_set");
        if (!saved)
            return reply_with_error(req, "Could not save the power state", restinio::status_internal_server_error());
        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()}});
    });

    // GET reads the mode; a GET that still names one is refused instead of applied.
    router->http_get("/operation_mode", [cfg](auto req, auto) {
        if (restinio::parse_query(req->header().query()).has("mode"))
            return reply_use_post(req);
        return reply_with_json(req, {{"operation_mode", cfg->get_operation_mode()}});
    });
    router->http_post("/operation_mode", [cfg](auto req, auto) {
        const auto mode = request_param(req, "mode");
        if (!mode)
            return reply_with_error(req, "No mode given");
        if (*mode != "automatic" && *mode != "manual")
            return reply_with_error(req, "Mode must be automatic or manual");
        const bool saved = cfg->set_operation_mode(*mode);
        PlaybackControl::restart("operation_mode");
        if (!saved)
            return reply_with_error(req, "Could not save the mode", restinio::status_internal_server_error());
        return reply_with_json(req, {{"operation_mode", cfg->get_operation_mode()}});
    });

    router->http_get("/status", [cfg](auto req, auto) {
        return reply_with_json(req, {{"turned_off", cfg->is_turned_off()},
                                     {"turned_on", !cfg->is_turned_off()},
                                     {"current", cfg->get_curr_id()},
                                     {"operation_mode", cfg->get_operation_mode()},
                                     {"director_mode", cfg->get_automatic_director_mode()},
                                     {"automatic_active", cfg->is_automatic_mode()},
                                     // The web UI reloads itself when this changes (daemon updated).
                                     {"version", Common::Version::getCurrentVersion().toString()}});
    });

    return router;
}

#include "halo_routes.h"

#include "halo/halo_service.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace {
/// Applies a partial update; returns an error text (empty on success).
std::string apply_halo_patch(const json &patch, Halo::Settings &settings)
{
    json merged = settings;
    for (const auto &[key, value] : patch.items()) {
        if (key != "enabled" && key != "mode" && key != "brightness" && key != "color" && key != "led_count"
            && key != "device" && key != "rotation" && key != "reverse")
            return "Unknown halo setting: " + key;
        merged[key] = value;
    }
    // from_json is lenient (wrong types fall back to defaults), so check the
    // types of what the client sent first.
    const auto type_ok = [&](const char *key, bool (json::*is)() const noexcept) {
        return !patch.contains(key) || (patch.at(key).*is)();
    };
    if (!type_ok("enabled", &json::is_boolean) || !type_ok("reverse", &json::is_boolean)
        || !type_ok("mode", &json::is_string) || !type_ok("color", &json::is_string)
        || !type_ok("device", &json::is_string) || !type_ok("brightness", &json::is_number_integer)
        || !type_ok("led_count", &json::is_number_integer) || !type_ok("rotation", &json::is_number_integer))
        return "Invalid type for a halo setting";
    Halo::Settings next = merged.get<Halo::Settings>();
    // Sanitizing would silently clamp; validate the raw values instead.
    Halo::Settings raw;
    raw.enabled = merged["enabled"];
    raw.mode = merged["mode"];
    raw.brightness = merged["brightness"];
    raw.color = merged["color"];
    raw.led_count = merged["led_count"];
    raw.device = merged["device"];
    raw.rotation = merged["rotation"];
    raw.reverse = merged["reverse"];
    if (const auto problem = raw.validate(); !problem.empty())
        return problem;
    settings = std::move(next);
    return {};
}
}  // namespace

std::unique_ptr<Server::router_t> Server::add_halo_routes(std::unique_ptr<router_t> router, Config::MainConfig* cfg) {
    router->http_get("/halo", [](const auto &req, auto) {
        return reply_with_json(req, Halo::Service::instance().status());
    });

    // Partial update: only the given keys change.
    router->http_post("/halo", [cfg](const auto &req, auto) {
        json patch;
        try {
            patch = json::parse(req->body());
        } catch (const std::exception &ex) {
            spdlog::warn("Invalid halo payload: {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }
        if (!patch.is_object())
            return reply_with_error(req, "Expected a JSON object");

        Halo::Settings settings = cfg->get_halo();
        if (const auto problem = apply_halo_patch(patch, settings); !problem.empty())
            return reply_with_error(req, problem);

        cfg->set_halo(settings);
        Halo::Service::instance().update_settings(cfg->get_halo());
        if (!cfg->save())
            return reply_with_error(req, "Could not save the halo settings", restinio::status_internal_server_error());
        return reply_with_json(req, Halo::Service::instance().status());
    });
    return router;
}

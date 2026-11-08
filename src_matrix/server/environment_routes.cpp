#include "environment_routes.h"

#include <atomic>
#include <thread>

#include "shared/matrix/environment/service.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

std::unique_ptr<Server::router_t> Server::add_environment_routes(std::unique_ptr<router_t> router) {
    router->http_get("/environment", [](const auto &req, auto) {
        return reply_with_json(req, Environment::Service::instance().status());
    });

    // Partial update: only the given keys change. Send "latitude": null to
    // clear the manual location (and fall back to the IP-detected one).
    router->http_post("/environment", [](const auto &req, auto) {
        json patch;
        try {
            patch = json::parse(req->body());
        } catch (const std::exception &ex) {
            spdlog::warn("Invalid environment payload: {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }
        if (!patch.is_object())
            return reply_with_error(req, "Expected a JSON object");

        json merged = config->get_environment();
        for (const auto &[key, value] : patch.items()) {
            if (key == "overrides" && value.is_object()) {
                for (const auto &[okey, ovalue] : value.items())
                    merged["overrides"][okey] = ovalue;
            } else if (key == "enabled" || key == "auto_locate" || key == "latitude" || key == "longitude" || key == "label") {
                merged[key] = value;
            }
        }
        Environment::Settings settings;
        try {
            settings = merged.get<Environment::Settings>();
        } catch (const std::exception &ex) {
            return reply_with_error(req, std::string("Invalid environment settings: ") + ex.what());
        }
        // A half-set manual location is dropped by sanitize(); say so.
        if ((patch.contains("latitude") && !patch["latitude"].is_null()) && !settings.has_manual_location())
            return reply_with_error(req, "latitude/longitude must both be valid numbers");

        config->set_environment(settings);
        config->save();
        Environment::Service::instance().update_settings(config->get_environment());
        return reply_with_json(req, Environment::Service::instance().status());
    });

    router->http_get("/environment/geocode", [](const auto &req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("q"))
            return reply_with_error(req, "Missing q");
        // The lookup takes up to seconds and the server has one worker thread:
        // answer from a background thread, at most a couple at a time.
        static std::atomic<int> running{0};
        if (running.fetch_add(1) >= 2) {
            running.fetch_sub(1);
            return reply_with_error(req, "Too many place searches at once, try again", restinio::status_too_many_requests());
        }
        std::thread([req, query = std::string(qp["q"])]() {
            try {
                const auto result = Environment::geocode(query);
                if (!result)
                    reply_with_error(req, result.error());
                else
                    reply_with_json(req, *result);
            } catch (const std::exception &e) {
                reply_with_error(req, e.what(), restinio::status_internal_server_error());
            }
            running.fetch_sub(1);
        }).detach();
        return restinio::request_accepted();
    });
    return router;
}

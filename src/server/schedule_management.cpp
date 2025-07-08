#include "schedule_management.h"
#include "shared/utils/shared.h"
#include "shared/server/server_utils.h"
#include "shared/utils/uuid.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using namespace spdlog;

std::unique_ptr<Server::router_t> Server::add_schedule_routes(std::unique_ptr<router_t> router) {
    // GET routes
    router->http_get("/schedules", [](const auto& req, auto) {
        const auto schedules = config->get_schedules();
        return reply_with_json(req, json(schedules));
    });

    router->http_get("/schedule", [](const auto& req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "No schedule ID given");
        }

        const string id{qp["id"]};
        const auto schedules = config->get_schedules();
        const auto it = schedules.find(id);

        if (it == schedules.end()) {
            return reply_with_error(req, "Schedule not found");
        }

        return reply_with_json(req, json(it->second));
    });

    router->http_get("/scheduling_status", [](const auto& req, auto) {
        const auto enabled = config->is_scheduling_enabled();
        const auto active_preset = config->get_active_scheduled_preset();

        return reply_with_json(req, {
                                   {"enabled", enabled},
                                   {"active_preset", active_preset.value_or("none")}
                               });
    });

    router->http_post("/scheduling_status", [](const auto& req, auto) {
        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
        } catch (exception &ex) {
            warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        if (!j["enabled"].is_boolean()) {
            return reply_with_error(req, "No enabled parameter given");
        }

        bool enabled = j["enabled"].get<bool>();
        config->set_scheduling_enabled(enabled);

        return reply_with_json(req, {{"enabled", enabled}});
    });

    // POST routes
    router->http_post("/schedule", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        string id;

        if (qp.has("id")) {
            id = qp["id"];
        } else {
            id = uuid::generate_uuid_v4();
        }

        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
            j["id"] = id;
        } catch (exception &ex) {
            warn("Invalid json payload {}", ex.what());
            return reply_with_error(req, "Invalid json payload");
        }

        try {
            ConfigData::Schedule schedule;
            j.get_to(schedule);
            schedule.id = id; // Ensure ID matches

            // Validate schedule data
            if (schedule.start_hour < 0 || schedule.start_hour > 23 ||
                schedule.end_hour < 0 || schedule.end_hour > 23 ||
                schedule.start_minute < 0 || schedule.start_minute > 59 ||
                schedule.end_minute < 0 || schedule.end_minute > 59) {
                return reply_with_error(req, "Invalid time values");
            }

            // Validate days of week
            for (const int day: schedule.days_of_week) {
                if (day < 0 || day > 6) {
                    return reply_with_error(req, "Invalid day of week");
                }
            }

            // Check if preset exists
            const auto presets = config->get_presets();
            if (!presets.contains(schedule.preset_id)) {
                return reply_with_error(req, "Preset not found");
            }

            config->set_schedule(id, schedule);
            return reply_with_json(req, {{"success", "Schedule saved"}, {"id", id}});
        } catch (exception &ex) {
            warn("Invalid schedule data {}", ex.what());
            return reply_with_error(req, "Could not parse schedule data");
        }
    });

    // DELETE routes
    router->http_delete("/schedule", [](auto req, auto) {
        const auto qp = restinio::parse_query(req->header().query());
        if (!qp.has("id")) {
            return reply_with_error(req, "No schedule ID given");
        }

        const string id{qp["id"]};
        const bool success = config->delete_schedule(id);

        if (!success) {
            return reply_with_error(req, "Schedule not found");
        }

        return reply_with_json(req, {{"success", "Schedule deleted"}});
    });

    return std::move(router);
}

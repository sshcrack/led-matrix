#pragma once
#include "server.h"

namespace Server {
    /**
     * Registers the schedule REST routes on `router` and hands it back:
     * GET /schedules, /schedule, /brightness_schedule, /scheduling_status and
     * POST /schedule, /brightness_schedule, /scheduling_status, DELETE /schedule.
     *
     * The POST and DELETE routes mutate the global `config` and mark it dirty, so the change is
     * persisted. POST /schedule validates times, weekdays and that the preset exists, and
     * generates a UUID when no `id` query parameter is given. The routes only store the
     * schedules; which preset is active is resolved later by `get_active_scheduled_preset()`.
     */
    std::unique_ptr<router_t> add_schedule_routes(std::unique_ptr<router_t> router);
}

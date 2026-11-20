#include "hardware_routes.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "hardware/panel_options.h"
#include "shared/common/utils/durable_file.h"
#include "shared/matrix/server/server_utils.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
// Shared with scripts/apply_hardware.sh and packaging/debian-matrix/led-matrix-hardware.path.
fs::path hardware_dir()
{
    return fs::path(LED_MATRIX_DATA_DIR) / "hardware";
}

json read_json(const fs::path& path)
{
    std::ifstream in(path);
    if (!in)
        return nullptr;
    auto j = json::parse(in, nullptr, false);
    return j.is_discarded() ? json(nullptr) : j;
}

// Empty when changes can be applied, else why not. App updates switch binaries
// only (docs/transactional-updates.md), so a Pi whose package predates the root
// helper has neither the script nor its path unit.
std::string unsupported_reason()
{
#ifdef ENABLE_EMULATOR
    return "The emulator takes its panel settings from the command line";
#else
    std::error_code ec;
    for (const char* dir : {"/lib/systemd/system", "/usr/lib/systemd/system", "/etc/systemd/system"})
        if (fs::exists(fs::path(dir) / "led-matrix-hardware.path", ec))
            return {};
    return "Changing these needs the led-matrix package installed again with apt or dpkg; app updates alone do not add the helper it needs";
#endif
}

json status()
{
    const auto running = PanelHardware::running();
    std::error_code ec;
    return {{"running", PanelHardware::to_json(running)},
            {"flags", PanelHardware::to_flags(running)},
            {"supported", unsupported_reason().empty()},
            {"unsupported_reason", unsupported_reason()},
            // Created by the panel image (build_generic_image.sh); removed once
            // someone has looked at the settings.
            {"setup_needed", fs::exists(hardware_dir() / "setup-needed", ec)},
            {"pending", fs::exists(hardware_dir() / "request.json", ec)},
            {"result", read_json(hardware_dir() / "state.json")}};
}

void mark_set_up()
{
    std::error_code ec;
    fs::remove(hardware_dir() / "setup-needed", ec);
}
}  // namespace

std::unique_ptr<Server::router_t> Server::add_hardware_routes(std::unique_ptr<router_t> router)
{
    router->http_get("/hardware", [](auto req, auto) { return reply_with_json(req, status()); });

    // {"options": {...}}: validated here and again by the root helper. The same
    // options as now only confirm them (no restart).
    router->http_post("/hardware", [](auto req, auto) {
        const auto body = json::parse(req->body(), nullptr, false);
        if (!body.is_object() || !body.contains("options"))
            return reply_with_error(req, "Expected {\"options\": {...}}");
        std::string error;
        const auto options = PanelHardware::from_json(body["options"], error);
        if (!options)
            return reply_with_error(req, error);
        if (error = PanelHardware::validate(*options); !error.empty())
            return reply_with_error(req, error);
        if (*options == PanelHardware::running()) {
            mark_set_up();
            auto reply = status();
            reply["restart"] = false;
            return reply_with_json(req, reply);
        }
        if (const auto why = unsupported_reason(); !why.empty())
            return reply_with_error(req, why, restinio::status_conflict());
        std::error_code ec;
        fs::create_directories(hardware_dir(), ec);
        // The helper checks the restarted daemon on this port.
        const char* port = std::getenv("PORT");
        const json request = {{"options", PanelHardware::to_json(*options)}, {"port", port ? port : "8080"}};
        if (!DurableFile::replace(hardware_dir() / "request.json", request.dump(2) + "\n", &error))
            return reply_with_error(req, "Could not queue the change: " + error, restinio::status_internal_server_error());
        spdlog::info("Panel hardware change queued: {}", PanelHardware::to_flags(*options));
        mark_set_up();
        auto reply = status();
        reply["restart"] = true;
        return reply_with_json(req, reply);
    });
    return router;
}

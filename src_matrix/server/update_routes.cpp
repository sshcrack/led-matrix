#include "update_routes.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/common/Version.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include <cpr/cpr.h>
#include <future>
#include <chrono>

using json = nlohmann::json;
using namespace spdlog;

namespace Server
{

    std::unique_ptr<router_t> add_update_routes(std::unique_ptr<router_t> router, Update::UpdateManager *update_manager)
    {
        if (!update_manager)
        {
            error("UpdateManager is null, update routes will not be functional");
            return router;
        }

        // GET /api/update/status - Get current update status and configuration
        router->http_get("/api/update/status", [update_manager](auto req, auto)
                         {
            try {
                json response;
                response["auto_update_enabled"] = update_manager->is_auto_update_enabled();
                response["check_interval_hours"] = update_manager->get_check_interval_hours();
                response["current_version"] = Common::Version::getCurrentVersion().toString();
                response["latest_version"] = update_manager->get_latest_version().value_or(Common::Version(0, 0, 0)).toString();
                response["update_available"] = update_manager->is_update_available();
                response["status"] = static_cast<int>(update_manager->get_status());
                response["error_message"] = update_manager->get_error_message();
                
                return reply_with_json(req, response);
            } catch (const std::exception& ex) {
                error("Error getting update status: {}", ex.what());
                return reply_with_error(req, "Internal server error", restinio::status_internal_server_error());
            } });

        // POST /api/update/check - Manually check for updates
        router->http_post("/api/update/check", [update_manager](auto req, auto)
                          {
            try {
                auto update_info = update_manager->manual_check_for_updates();
                
                json response;
                if (update_info.has_value()) {
                    response["update_available"] = true;
                    response["version"] = update_info->version.toString();
                    response["download_url"] = update_info->download_url;
                    response["body"] = update_info->body;
                    response["is_prerelease"] = update_info->is_prerelease;
                } else {
                    response["update_available"] = false;
                    response["message"] = "No updates available";
                }
                
                return reply_with_json(req, response);
            } catch (const std::exception& ex) {
                error("Error checking for updates: {}", ex.what());
                return reply_with_error(req, "Failed to check for updates", restinio::status_internal_server_error());
            } });

        // POST /api/update/install - Install available update
        router->http_post("/api/update/install", [update_manager](auto req, auto)
                          {
            try {
                // Check if another installation is already in progress
                auto current_status = update_manager->get_status();
                if (current_status == Update::UpdateStatus::DOWNLOADING || 
                    current_status == Update::UpdateStatus::INSTALLING) {
                    json response;
                    response["message"] = "Update installation already in progress";
                    response["status"] = "already_running";
                    return reply_with_json(req, response);
                }
                
                // Parse query parameters
                const auto qp = restinio::parse_query(req->header().query());
                std::string version = qp.has("version") ? std::string{qp["version"]} : "";
                
                // Create a shared promise to track completion
                auto completion_promise = std::make_shared<std::promise<std::expected<void, std::string>>>();
                auto completion_future = completion_promise->get_future();
                
                // Set up pre-restart callback to signal completion
                update_manager->set_pre_restart_callback([completion_promise]() {
                    completion_promise->set_value({});
                });

                auto parsed_version = Common::Version::fromString(version);
                if(parsed_version.isInvalid())
                    return reply_with_error(req, "Invalid version format", restinio::status_bad_request());

                // Start installation in background thread
                std::thread([update_manager, parsed_version, completion_promise]() {
                    auto update = update_manager->get_update_info(parsed_version);
                    if(!update) {
                        spdlog::error("Failed to get update info: {}", update.error());
                        try {
                            completion_promise->set_value(std::unexpected(update.error()));
                        } catch (const std::future_error&) {
                            // Promise already set by pre-restart callback
                        }

                        return;
                    }

                    auto res = update_manager->manual_download_and_install(update.value());
                    if (!res) {
                        // If installation failed, signal completion with false
                        try {
                            completion_promise->set_value(res);
                        } catch (const std::future_error&) {
                            // Promise already set by pre-restart callback
                        }
                    }
                }).detach();
                
                // Wait for completion or timeout (30 seconds for download + install)
                auto wait_status = completion_future.wait_for(std::chrono::seconds(60));
                
                json response;
                if (wait_status == std::future_status::ready) {
                    std::expected<void, std::string> res = completion_future.get();
                    if (res) {
                        response["message"] = "Update installation completed successfully";
                        response["status"] = "success";
                        info("Update installation API call completed successfully");
                    } else {
                        response["message"] = res.error();
                        response["status"] = "error";
                        error("Update installation API call failed");
                    }
                } else {
                    // Timeout - installation is still running
                    response["message"] = "Update installation started (may restart service)";
                    response["status"] = "started";
                    warn("Update installation API call timed out - installation may still be running");
                }
                
                // Clear the callback
                update_manager->set_pre_restart_callback(nullptr);
                
                return reply_with_json(req, response);
            } catch (const std::exception& ex) {
                error("Error starting update installation: {}", ex.what());
                return reply_with_error(req, "Failed to start update installation", restinio::status_internal_server_error());
            } });

        // POST /api/update/config - Update configuration
        router->http_post("/api/update/config", [update_manager](auto req, auto)
                          {
            try {
                auto body_str = req->body();
                if (body_str.empty()) {
                    return reply_with_error(req, "Empty request body");
                }
                
                auto json_body = json::parse(body_str);
                
                // Update auto-update setting
                if (json_body.contains("auto_update_enabled")) {
                    bool enabled = json_body["auto_update_enabled"];
                    update_manager->set_auto_update_enabled(enabled);
                    info("Auto-update enabled set to: {}", enabled);
                }
                
                // Update check interval
                if (json_body.contains("check_interval_hours")) {
                    int hours = json_body["check_interval_hours"];
                    if (hours < 1 || hours > 168) { // Between 1 hour and 1 week
                        return reply_with_error(req, "Check interval must be between 1 and 168 hours");
                    }
                    update_manager->set_check_interval_hours(hours);
                    info("Update check interval set to: {} hours", hours);
                }
                
                json response;
                response["message"] = "Update configuration updated successfully";
                response["auto_update_enabled"] = update_manager->is_auto_update_enabled();
                response["check_interval_hours"] = update_manager->get_check_interval_hours();
                
                return reply_with_json(req, response);
            } catch (const std::exception& ex) {
                error("Error updating configuration: {}", ex.what());
                return reply_with_error(req, "Failed to update configuration", restinio::status_internal_server_error());
            } });

        // GET /api/update/releases - Get recent releases from GitHub
        router->http_get("/api/update/releases", [](auto req, auto)
                         {
            try {
                const auto qp = restinio::parse_query(req->header().query());
                int per_page = qp.has("per_page") ? std::stoi(std::string{qp["per_page"]}) : 5;
                
                if (per_page < 1 || per_page > 20) {
                    per_page = 5; // Default to 5 releases
                }
                
                std::string api_url = "https://api.github.com/repos/sshcrack/led-matrix/releases?per_page=" + std::to_string(per_page);
                auto response = cpr::Get(cpr::Url{api_url});
                
                if (response.status_code != 200) {
                    return reply_with_error(req, "Failed to fetch releases from GitHub", restinio::status_service_unavailable());
                }
                
                auto releases_json = json::parse(response.text);
                json simplified_releases = json::array();
                
                for (const auto& release : releases_json) {
                    json simplified;
                    simplified["version"] = release["tag_name"];
                    simplified["name"] = release["name"];
                    simplified["body"] = release["body"];
                    simplified["published_at"] = release["published_at"];
                    simplified["is_prerelease"] = release["prerelease"];
                    simplified["is_draft"] = release["draft"];
                    
                    // Find the led-matrix Linux asset
                    for (const auto& asset : release["assets"]) {
                        std::string asset_name = asset["name"];
                        if (asset_name.find("led-matrix") != std::string::npos && 
                            asset_name.find("arm64") != std::string::npos &&
                            asset_name.ends_with(".tar.gz")) {
                            simplified["download_url"] = asset["browser_download_url"];
                            simplified["download_size"] = asset["size"];
                            break;
                        }
                    }
                    
                    simplified_releases.push_back(simplified);
                }
                
                return reply_with_json(req, simplified_releases);
            } catch (const std::exception& ex) {
                error("Error fetching releases: {}", ex.what());
                return reply_with_error(req, "Failed to fetch releases", restinio::status_internal_server_error());
            } });

        return router;
    }
}
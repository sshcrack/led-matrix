#include "marketplace_routes.h"
#include "shared/common/marketplace/client.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>

using namespace restinio;
using json = nlohmann::json;
using namespace Plugins::Marketplace;

namespace Server {

// Global marketplace client instance
static std::unique_ptr<MarketplaceClient> marketplace_client;
static std::once_flag client_init_flag;

// Initialize marketplace client
void init_marketplace_client() {
    std::call_once(client_init_flag, []() {
        auto exec_dir = get_exec_dir();
        auto raw_plugin = getenv("PLUGIN_DIR");
        
        std::filesystem::path plugin_dir = exec_dir / "plugins";
        if (raw_plugin != nullptr) {
            plugin_dir = std::filesystem::path(raw_plugin);
        }
        
        marketplace_client = create_marketplace_client(plugin_dir.string());
        spdlog::info("Marketplace client initialized with plugin directory: {}", plugin_dir.string());
    });
}

std::unique_ptr<router_t> add_marketplace_routes(std::unique_ptr<router_t> router) {
    init_marketplace_client();
    
    // GET /marketplace/index - Get marketplace index
    router->http_get("/marketplace/index", [](const auto& req, auto) {
        try {
            auto cached_index = marketplace_client->get_cached_index();
            if (!cached_index.has_value()) {
                return Server::reply_with_error(req, "No marketplace index available", status_not_found());
            }
            
            json response = cached_index.value();
            return Server::reply_with_json(req, response);
                
        } catch (const std::exception& e) {
            spdlog::error("Error getting marketplace index: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/refresh - Refresh marketplace index
    router->http_post("/marketplace/refresh", [](const auto& req, auto) {
        try {
            auto query_params = parse_query(req->header().query());
            std::string index_url = "";
            
            auto url_it = query_params.find("url");
            if (url_it != query_params.end()) {
                index_url = url_it->second;
            }
            
            // Start async fetch
            auto future = marketplace_client->fetch_index(index_url);
            
            json response;
            response["message"] = "Index refresh started";
            return Server::reply_with_json(req, response, status_accepted());
                
        } catch (const std::exception& e) {
            spdlog::error("Error refreshing marketplace index: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // GET /marketplace/installed - Get installed plugins
    router->http_get("/marketplace/installed", [](const auto& req, auto) {
        try {
            auto installed = marketplace_client->get_installed_plugins();
            json response = installed;
            
            return req->create_response(status_ok())
                .set_header(field::content_type, "application/json")
                .set_header(field::access_control_allow_origin, "*")
                .set_body(response.dump());
                
        } catch (const std::exception& e) {
            spdlog::error("Error getting installed plugins: {}", e.what());
            return req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
        }
    });
    
    // GET /marketplace/status/{plugin_id} - Get plugin installation status
    router->http_get(R"(/marketplace/status/([a-zA-Z0-9\-_]+))", [](const auto& req, auto) {
        try {
            std::string plugin_id = req->route_params()["id"].to_string();
            auto status = marketplace_client->get_plugin_status(plugin_id);
            
            json response;
            response["plugin_id"] = plugin_id;
            response["status"] = static_cast<int>(status);
            
            // Add status string for easier frontend handling
            switch (status) {
                case InstallationStatus::NOT_INSTALLED:
                    response["status_string"] = "not_installed";
                    break;
                case InstallationStatus::INSTALLED:
                    response["status_string"] = "installed";
                    break;
                case InstallationStatus::UPDATE_AVAILABLE:
                    response["status_string"] = "update_available";
                    break;
                case InstallationStatus::DOWNLOADING:
                    response["status_string"] = "downloading";
                    break;
                case InstallationStatus::INSTALLING:
                    response["status_string"] = "installing";
                    break;
                case InstallationStatus::ERROR:
                    response["status_string"] = "error";
                    break;
            }
            
            return req->create_response(status_ok())
                .set_header(field::content_type, "application/json")
                .set_header(field::access_control_allow_origin, "*")
                .set_body(response.dump());
                
        } catch (const std::exception& e) {
            spdlog::error("Error getting plugin status: {}", e.what());
            return req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
        }
    });
    
    // POST /marketplace/install - Install a plugin
    router->http_post("/marketplace/install", [](const auto& req, auto) -> restinio::request_handling_status_t {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id") || !request_data.contains("version")) {
                auto response = req->create_response(status_bad_request())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Missing plugin_id or version"})");
                return response.done();
            }
            
            std::string plugin_id = request_data["plugin_id"];
            std::string version = request_data["version"];
            
            // Find plugin in index
            auto cached_index = marketplace_client->get_cached_index();
            if (!cached_index.has_value()) {
                auto response = req->create_response(status_not_found())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "No marketplace index available"})");
                return response.done();
            }
            
            auto plugin_it = std::find_if(cached_index->plugins.begin(), cached_index->plugins.end(),
                                         [&plugin_id](const PluginInfo& p) { return p.id == plugin_id; });
            
            if (plugin_it == cached_index->plugins.end()) {
                auto response = req->create_response(status_not_found())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Plugin not found"})");
                return response.done();
            }
            
            // Start installation (async)
            auto future = marketplace_client->install_plugin(*plugin_it, version, nullptr, nullptr);
            
            json response_json;
            response_json["message"] = "Installation started";
            response_json["plugin_id"] = plugin_id;
            response_json["version"] = version;
            
            auto response = req->create_response(status_accepted())
                .set_header(field::content_type, "application/json")
                .set_header(field::access_control_allow_origin, "*")
                .set_body(response_json.dump());
            return response.done();
                
        } catch (const std::exception& e) {
            spdlog::error("Error installing plugin: {}", e.what());
            auto response = req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
            return response.done();
        }
    });
    
    // POST /marketplace/uninstall - Uninstall a plugin
    router->http_post("/marketplace/uninstall", [](const auto& req, auto) -> restinio::request_handling_status_t {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                auto response = req->create_response(status_bad_request())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Missing plugin_id"})");
                return response.done();
            }
            
            std::string plugin_id = request_data["plugin_id"];
            
            // Start uninstallation (async)
            auto future = marketplace_client->uninstall_plugin(plugin_id, nullptr);
            
            json response_json;
            response_json["message"] = "Uninstallation started";
            response_json["plugin_id"] = plugin_id;
            
            auto response = req->create_response(status_accepted())
                .set_header(field::content_type, "application/json")
                .set_header(field::access_control_allow_origin, "*")
                .set_body(response_json.dump());
            return response.done();
                
        } catch (const std::exception& e) {
            spdlog::error("Error uninstalling plugin: {}", e.what());
            auto response = req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
            return response.done();
        }
    });
    
    // POST /marketplace/enable - Enable a plugin
    router->http_post("/marketplace/enable", [](const auto& req, auto) -> restinio::request_handling_status_t {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                auto response = req->create_response(status_bad_request())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Missing plugin_id"})");
                return response.done();
            }
            
            std::string plugin_id = request_data["plugin_id"];
            bool success = marketplace_client->enable_plugin(plugin_id);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin enabled";
                response_json["plugin_id"] = plugin_id;
                
                auto response = req->create_response(status_ok())
                    .set_header(field::content_type, "application/json")
                    .set_header(field::access_control_allow_origin, "*")
                    .set_body(response_json.dump());
                return response.done();
            } else {
                auto response = req->create_response(status_not_found())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Plugin not found"})");
                return response.done();
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error enabling plugin: {}", e.what());
            auto response = req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
            return response.done();
        }
    });
    
    // POST /marketplace/disable - Disable a plugin
    router->http_post("/marketplace/disable", [](const auto& req, auto) -> restinio::request_handling_status_t {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                auto response = req->create_response(status_bad_request())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Missing plugin_id"})");
                return response.done();
            }
            
            std::string plugin_id = request_data["plugin_id"];
            bool success = marketplace_client->disable_plugin(plugin_id);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin disabled";
                response_json["plugin_id"] = plugin_id;
                
                auto response = req->create_response(status_ok())
                    .set_header(field::content_type, "application/json")
                    .set_header(field::access_control_allow_origin, "*")
                    .set_body(response_json.dump());
                return response.done();
            } else {
                auto response = req->create_response(status_not_found())
                    .set_header(field::content_type, "application/json")
                    .set_body(R"({"error": "Plugin not found"})");
                return response.done();
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error disabling plugin: {}", e.what());
            auto response = req->create_response(status_internal_server_error())
                .set_header(field::content_type, "application/json")
                .set_body(R"({"error": "Internal server error"})");
            return response.done();
        }
    });
    
    return router;
}

} // namespace Server
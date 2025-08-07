#include "marketplace_routes.h"
#include "shared/common/marketplace/client.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/plugin_loader/loader.h"
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
            auto query_params = restinio::parse_query(req->header().query());
            std::string index_url = "";
            
            if (query_params.has("url")) {
                index_url = std::string(query_params["url"]);
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
            return Server::reply_with_json(req, response);
                
        } catch (const std::exception& e) {
            spdlog::error("Error getting installed plugins: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // GET /marketplace/status/{plugin_id} - Get plugin installation status
    router->http_get(R"(/marketplace/status/([a-zA-Z0-9\-_]+))", [](const auto& req, auto params) {
        try {
            std::string plugin_id = std::string(params["id"]);
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
            
            return Server::reply_with_json(req, response);
                
        } catch (const std::exception& e) {
            spdlog::error("Error getting plugin status: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/install - Install a plugin
    router->http_post("/marketplace/install", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id") || !request_data.contains("version")) {
                return Server::reply_with_error(req, "Missing plugin_id or version");
            }
            
            std::string plugin_id = request_data["plugin_id"];
            std::string version = request_data["version"];
            
            // Find plugin in index
            auto cached_index = marketplace_client->get_cached_index();
            if (!cached_index.has_value()) {
                return Server::reply_with_error(req, "No marketplace index available", status_not_found());
            }
            
            auto plugin_it = std::find_if(cached_index->plugins.begin(), cached_index->plugins.end(),
                                         [&plugin_id](const PluginInfo& p) { return p.id == plugin_id; });
            
            if (plugin_it == cached_index->plugins.end()) {
                return Server::reply_with_error(req, "Plugin not found", status_not_found());
            }
            
            // Start installation (async)
            auto future = marketplace_client->install_plugin(*plugin_it, version, nullptr, nullptr);
            
            json response_json;
            response_json["message"] = "Installation started";
            response_json["plugin_id"] = plugin_id;
            response_json["version"] = version;
            
            return Server::reply_with_json(req, response_json, status_accepted());
                
        } catch (const std::exception& e) {
            spdlog::error("Error installing plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/uninstall - Uninstall a plugin
    router->http_post("/marketplace/uninstall", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                return Server::reply_with_error(req, "Missing plugin_id");
            }
            
            std::string plugin_id = request_data["plugin_id"];
            
            // Start uninstallation (async)
            auto future = marketplace_client->uninstall_plugin(plugin_id, nullptr);
            
            json response_json;
            response_json["message"] = "Uninstallation started";
            response_json["plugin_id"] = plugin_id;
            
            return Server::reply_with_json(req, response_json, status_accepted());
                
        } catch (const std::exception& e) {
            spdlog::error("Error uninstalling plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/enable - Enable a plugin
    router->http_post("/marketplace/enable", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                return Server::reply_with_error(req, "Missing plugin_id");
            }
            
            std::string plugin_id = request_data["plugin_id"];
            bool success = marketplace_client->enable_plugin(plugin_id);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin enabled";
                response_json["plugin_id"] = plugin_id;
                return Server::reply_with_json(req, response_json);
            } else {
                return Server::reply_with_error(req, "Plugin not found", status_not_found());
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error enabling plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/disable - Disable a plugin
    router->http_post("/marketplace/disable", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                return Server::reply_with_error(req, "Missing plugin_id");
            }
            
            std::string plugin_id = request_data["plugin_id"];
            bool success = marketplace_client->disable_plugin(plugin_id);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin disabled";
                response_json["plugin_id"] = plugin_id;
                return Server::reply_with_json(req, response_json);
            } else {
                return Server::reply_with_error(req, "Plugin not found", status_not_found());
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error disabling plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/load - Load a plugin dynamically
    router->http_post("/marketplace/load", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_path")) {
                return Server::reply_with_error(req, "Missing plugin_path");
            }
            
            std::string plugin_path = request_data["plugin_path"];
            bool success = Plugins::PluginManager::instance()->load_plugin(plugin_path);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin loaded successfully";
                response_json["plugin_path"] = plugin_path;
                return Server::reply_with_json(req, response_json);
            } else {
                return Server::reply_with_error(req, "Failed to load plugin", status_internal_server_error());
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error loading plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    // POST /marketplace/unload - Unload a plugin dynamically
    router->http_post("/marketplace/unload", [](const auto& req, auto) {
        try {
            auto body = req->body();
            json request_data = json::parse(body);
            
            if (!request_data.contains("plugin_id")) {
                return Server::reply_with_error(req, "Missing plugin_id");
            }
            
            std::string plugin_id = request_data["plugin_id"];
            bool success = Plugins::PluginManager::instance()->unload_plugin(plugin_id);
            
            if (success) {
                json response_json;
                response_json["message"] = "Plugin unloaded successfully";
                response_json["plugin_id"] = plugin_id;
                return Server::reply_with_json(req, response_json);
            } else {
                return Server::reply_with_error(req, "Failed to unload plugin or plugin not found", status_not_found());
            }
                
        } catch (const std::exception& e) {
            spdlog::error("Error unloading plugin: {}", e.what());
            return Server::reply_with_error(req, "Internal server error", status_internal_server_error());
        }
    });
    
    return router;
}

} // namespace Server
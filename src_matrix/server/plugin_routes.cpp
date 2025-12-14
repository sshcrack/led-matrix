#include "plugin_routes.h"
#include "shared/matrix/plugin_loader/PluginStore.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>

using namespace Plugins;
using namespace restinio;

namespace Server
{
    std::unique_ptr<router_t> add_plugin_routes(std::unique_ptr<router_t> router)
    {

        router->http_get("/api/plugins/store", [](auto req, auto params)
                         {
            std::string owner = "sshcrack";
            std::string repo = "led-matrix-plugins";

            auto& store = PluginStore::instance();
            auto result = store.get_releases(owner, repo);
            
            if (!result) {
                return reply_with_error(req, result.error(), status_internal_server_error());
            }

            nlohmann::json releases = nlohmann::json::array();
            for(const auto& r : *result) {
                 releases.push_back({
                    {"version", r.version},
                    {"tag_name", r.tag_name},
                    {"download_url", r.download_url},
                    {"name", r.name},
                    {"body", r.body},
                    {"prerelease", r.prerelease},
                    {"published_at", r.published_at},
                    {"size", r.size}
                 });
            }
            return reply_with_json(req, releases); });

        router->http_post("/api/plugins/install", [](auto req, auto params)
                          {

            try {
                auto body = req->body();
                nlohmann::json j = nlohmann::json::parse(body);
                if (!j.contains("url") || !j.contains("filename")) {
                    return reply_with_error(req, "Missing url or filename");
                }
                std::string url = j["url"].template get<std::string>();
                std::string filename = j["filename"].template get<std::string>();

                auto& store = PluginStore::instance();
                auto result = store.install_plugin(url, filename);

                if (!result) {
                    return reply_with_error(req, result.error(), status_internal_server_error());
                }
                return reply_success(req);
            } catch (const std::exception& e) {
                 return reply_with_error(req, e.what(), status_bad_request());
            } });

        return router;
    }
}

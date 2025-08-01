#include "desktop_ws.h"
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared/matrix/plugin_loader/loader.h>

#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>

std::unique_ptr<router_t> Server::add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t &registry)
{
    router->http_get("/desktopWebsocket", [&registry](auto req, auto)
                     {
        spdlog::info("WebSocket connection request received.");
        if (restinio::http_connection_header_t::upgrade == req->header().connection()) {
            auto wsh =
                    rws::upgrade<traits_t>(
                        *req,
                        rws::activation_t::immediate,
                        [ &registry ](auto wsh, auto m) {
                            if(rws::opcode_t::text_frame == m->opcode()) {
                                std::string mStr = m->payload();
                                if (mStr.starts_with("msg:")) {
                                    const int pluginNameEnd = mStr.find(':', 4);

                                    const std::string pluginName = mStr.substr(4, pluginNameEnd -4);
                                    const std::string message = mStr.substr(mStr.find(':', pluginNameEnd) +1);

                                    for (const auto & plugin : Plugins::PluginManager::instance()->get_plugins()) {
                                        if (plugin->get_plugin_name() != pluginName)
                                            continue;

                                        plugin->on_websocket_message(message);
                                    }
                            }
                            }
                            if (rws::opcode_t::ping_frame == m->opcode()) {
                                auto resp = *m;
                                resp.set_opcode(rws::opcode_t::pong_frame);
                                wsh->send_message(resp);
                            } else if (rws::opcode_t::connection_close_frame == m->opcode()) {
                                std::unique_lock lock(registryMutex);
                                registry.erase(wsh->connection_id());
                            }
                        });
            // Store websocket handle to registry object to prevent closing of the websocket
            // on exit from this request handler.

            {
                std::unique_lock lock(registryMutex);
                registry.emplace(wsh->connection_id(), wsh);
            } // Release registryMutex here

            std::string sceneName;
            {
                std::unique_lock lock1(currSceneMutex);
                if(currScene != nullptr)
                    sceneName = currScene->get_name();
            } // Release currSceneMutex here

            namespace rws = restinio::websocket::basic;
            rws::message_t message;
            message.set_opcode(rws::opcode_t::text_frame);
            message.set_final_flag(rws::final_frame_flag_t::final_frame);
            message.set_payload("active:" + sceneName);

            wsh->send_message(message);

            for (const auto &plugin: Plugins::PluginManager::instance()->get_plugins()) {
                auto msgs = plugin->on_websocket_open();
                if (!msgs.has_value())
                    continue;

                for (const auto &msg: msgs.value()) {
                    message.set_payload("msg:" + plugin->get_plugin_name() + ":" + msg);
                    wsh->send_message(message);
                }
            }

            return restinio::request_accepted();
        }

        spdlog::warn("WebSocket connection request rejected: Connection header is not upgrade.");
        return restinio::request_rejected(); });
    return std::move(router);
}

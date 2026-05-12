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
                                    const auto pluginNameEnd = mStr.find(':', 4);
                                    if (pluginNameEnd == std::string::npos) {
                                        spdlog::warn("Discarding malformed plugin message (missing plugin delimiter): {}", mStr);
                                        return;
                                    }

                                    const auto messageStart = pluginNameEnd + 1;
                                    if (messageStart > mStr.size()) {
                                        spdlog::warn("Discarding malformed plugin message (invalid payload delimiter): {}", mStr);
                                        return;
                                    }

                                    const std::string pluginName = mStr.substr(4, pluginNameEnd - 4);
                                    const std::string message = mStr.substr(messageStart);

                                    for (const auto & plugin : Plugins::PluginManager::instance()->get_plugins()) {
                                        if (plugin->get_plugin_name() != pluginName)
                                            continue;

                                        try {
                                            plugin->on_websocket_message(message);
                                        } catch (const std::exception &ex) {
                                            spdlog::error("Plugin '{}' websocket handler failed: {}", plugin->get_plugin_name(), ex.what());
                                        } catch (...) {
                                            spdlog::error("Plugin '{}' websocket handler failed with unknown exception", plugin->get_plugin_name());
                                        }
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
                                desktop_connection_count--;
                            }
                        });
            // Store websocket handle to registry object to prevent closing of the websocket
            // on exit from this request handler.

            {
                std::unique_lock lock(registryMutex);
                registry.emplace(wsh->connection_id(), wsh);
                desktop_connection_count++;
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
                std::optional<std::vector<std::string>> msgs;
                try {
                    msgs = plugin->on_websocket_open();
                } catch (const std::exception &ex) {
                    spdlog::error("Plugin '{}' websocket-open hook failed: {}", plugin->get_plugin_name(), ex.what());
                    continue;
                } catch (...) {
                    spdlog::error("Plugin '{}' websocket-open hook failed with unknown exception", plugin->get_plugin_name());
                    continue;
                }
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

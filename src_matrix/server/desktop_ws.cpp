#include "desktop_ws.h"
#include <algorithm>
#include <unordered_set>
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/common/desktop_control_protocol.h>

#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>

namespace {
std::unordered_set<restinio::connection_id_t> scene_worker_connections;

void publish_desktop_runtime_input(bool availability_changed)
{
    const auto connections = std::max(0, Server::desktop_connection_count.load());
    RuntimeInputs::set_available(
        RuntimeInputIds::Desktop,
        connections > 0,
        {{"connected", connections > 0},
         {"connections", static_cast<std::int64_t>(connections)}});
    if (availability_changed)
        exit_canvas_update.store(true);
}
} // namespace

void Server::broadcast_matrix_enabled(bool enabled)
{
    std::shared_lock lock(registryMutex);
    for (const auto &[connection_id, handle] : registry) {
        if (scene_worker_connections.contains(connection_id))
            continue;

        rws::message_t message;
        message.set_opcode(rws::opcode_t::text_frame);
        message.set_final_flag(rws::final_frame_flag_t::final_frame);
        message.set_payload(DesktopControlProtocol::matrix_enabled(enabled));
        handle->send_message(message);
    }
}

std::unique_ptr<router_t> Server::add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t &registry)
{
    publish_desktop_runtime_input(false);
    router->http_get("/desktopWebsocket", [&registry](auto req, auto)
                     {
        spdlog::info("WebSocket connection request received.");
        if (restinio::http_connection_header_t::upgrade == req->header().connection()) {
            const auto query = restinio::parse_query(req->header().query());
            const bool is_scene_worker = query.has("role")
                && std::string{query["role"]} == "scene-worker";
            auto wsh =
                    rws::upgrade<traits_t>(
                        *req,
                        rws::activation_t::immediate,
                        [ &registry, is_scene_worker ](auto wsh, auto m) {
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
                                if (registry.erase(wsh->connection_id()) > 0) {
                                    if (scene_worker_connections.erase(wsh->connection_id()) == 0) {
                                        const int previous = desktop_connection_count.fetch_sub(1);
                                        if (previous <= 1)
                                            desktop_connection_count.store(0);
                                        publish_desktop_runtime_input(previous == 1);
                                    }
                                }
                            }
                        });
            // Store websocket handle to registry object to prevent closing of the websocket
            // on exit from this request handler.

            {
                std::unique_lock lock(registryMutex);
                const auto [_, inserted] = registry.emplace(wsh->connection_id(), wsh);
                if (inserted) {
                    if (is_scene_worker) {
                        scene_worker_connections.insert(wsh->connection_id());
                    } else {
                        const int previous = desktop_connection_count.fetch_add(1);
                        publish_desktop_runtime_input(previous == 0);
                    }
                }
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

            message.set_payload(DesktopControlProtocol::matrix_enabled(!config->is_turned_off()));
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

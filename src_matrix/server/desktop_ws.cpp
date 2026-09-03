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
         {"connections", static_cast<std::int64_t>(connections)},
         {"producer_owner_connection", static_cast<std::int64_t>(Server::desktop_producer_owner())}});
    if (availability_changed)
        exit_canvas_update.store(true);
}

void send_plugin_message(const Server::rws::ws_handle_t& socket,
                         const std::string& plugin_name,
                         const std::string& payload)
{
    if (!socket)
        return;
    Server::rws::message_t message;
    message.set_opcode(Server::rws::opcode_t::text_frame);
    message.set_final_flag(Server::rws::final_frame_flag_t::final_frame);
    message.set_payload("msg:" + plugin_name + ":" + payload);
    socket->send_message(std::move(message));
}

void send_control_message(const Server::rws::ws_handle_t& socket, const std::string& payload)
{
    if (!socket)
        return;
    Server::rws::message_t message;
    message.set_opcode(Server::rws::opcode_t::text_frame);
    message.set_final_flag(Server::rws::final_frame_flag_t::final_frame);
    message.set_payload(payload);
    socket->send_message(std::move(message));
}

Server::rws::ws_handle_t socket_for(const std::uint64_t connection_id)
{
    std::shared_lock lock(Server::registryMutex);
    const auto it = Server::registry.find(connection_id);
    return it == Server::registry.end() ? Server::rws::ws_handle_t{} : it->second;
}

void send_producer_role(const std::uint64_t connection_id, const bool active)
{
    const auto socket = socket_for(connection_id);
    if (!socket)
        return;

    // This transport-level gate applies to every UDP-producing desktop plugin,
    // including AudioVisualizer, not just plugins with matrix->desktop commands.
    send_control_message(socket, DesktopControlProtocol::desktop_producer(active));

    for (const auto& plugin : Plugins::PluginManager::instance()->get_plugins()) {
        if (!plugin->requires_single_desktop_producer())
            continue;
        send_plugin_message(socket, plugin->get_plugin_name(), active ? "producer:active" : "producer:standby");
    }
}

void send_initial_plugin_messages(const Server::rws::ws_handle_t& socket,
                                  const bool producer_owner,
                                  const bool producer_only = false)
{
    if (!socket)
        return;
    for (const auto& plugin : Plugins::PluginManager::instance()->get_plugins()) {
        const bool exclusive = plugin->requires_single_desktop_producer();
        if (producer_only && !exclusive)
            continue;
        if (exclusive && !producer_owner)
            continue;

        const auto messages = plugin->on_websocket_open();
        if (!messages.has_value())
            continue;
        for (const auto& payload : *messages)
            send_plugin_message(socket, plugin->get_plugin_name(), payload);
    }
}

void apply_producer_change(const Server::DesktopProducerChange& change,
                           const bool replay_new_owner)
{
    if (!change.changed)
        return;

    if (change.previous_owner != 0 && change.previous_owner != change.owner)
        send_producer_role(change.previous_owner, false);

    if (change.owner != 0) {
        send_producer_role(change.owner, true);
        if (replay_new_owner)
            send_initial_plugin_messages(socket_for(change.owner), true, true);
    }

    // The connection count may be unchanged when ownership moves, but expose
    // the new owner in Runtime Inputs/diagnostics immediately.
    publish_desktop_runtime_input(false);
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
    clear_desktop_producers();
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
                                const std::string mStr = m->payload();
                                if (mStr.starts_with("msg:")) {
                                    const auto pluginNameEnd = mStr.find(':', 4);
                                    if (pluginNameEnd == std::string::npos)
                                        return;

                                    const std::string pluginName = mStr.substr(4, pluginNameEnd - 4);
                                    const std::string message = mStr.substr(pluginNameEnd + 1);

                                    for (const auto & plugin : Plugins::PluginManager::instance()->get_plugins()) {
                                        if (plugin->get_plugin_name() != pluginName)
                                            continue;

                                        if (plugin->requires_single_desktop_producer()
                                            && !Server::accepts_desktop_producer_message(wsh->connection_id())) {
                                            spdlog::debug("Ignoring {} message from standby desktop connection {}",
                                                          pluginName, wsh->connection_id());
                                            break;
                                        }

                                        plugin->on_websocket_message(message);
                                        break;
                                    }
                                }
                            }
                            if (rws::opcode_t::ping_frame == m->opcode()) {
                                auto resp = *m;
                                resp.set_opcode(rws::opcode_t::pong_frame);
                                wsh->send_message(resp);
                            } else if (rws::opcode_t::connection_close_frame == m->opcode()) {
                                bool removed_regular_desktop = false;
                                int previous_count = 0;
                                {
                                    std::unique_lock lock(registryMutex);
                                    if (registry.erase(wsh->connection_id()) > 0) {
                                        if (scene_worker_connections.erase(wsh->connection_id()) == 0) {
                                            removed_regular_desktop = true;
                                            previous_count = desktop_connection_count.fetch_sub(1);
                                            if (previous_count <= 1)
                                                desktop_connection_count.store(0);
                                        }
                                    }
                                }

                                if (removed_regular_desktop) {
                                    const auto change = unregister_desktop_producer(wsh->connection_id());
                                    publish_desktop_runtime_input(previous_count == 1);
                                    // A clean owner disconnect promotes the newest remaining
                                    // controller and replays the current producer request.
                                    apply_producer_change(change, true);
                                }
                            }
                        });
            // Store websocket handle to registry object to prevent closing of the websocket
            // on exit from this request handler.

            bool inserted_regular_desktop = false;
            int previous_count = 0;
            {
                std::unique_lock lock(registryMutex);
                const auto [_, inserted] = registry.emplace(wsh->connection_id(), wsh);
                if (inserted) {
                    if (is_scene_worker) {
                        scene_worker_connections.insert(wsh->connection_id());
                    } else {
                        inserted_regular_desktop = true;
                        previous_count = desktop_connection_count.fetch_add(1);
                    }
                }
            } // Release registryMutex here

            if (inserted_regular_desktop) {
                // The newest controller owns single-producer desktop plugins. A
                // reconnect therefore supersedes any stale server-side socket.
                const auto change = register_desktop_producer(wsh->connection_id());
                publish_desktop_runtime_input(previous_count == 0);
                apply_producer_change(change, false);
            }

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

            const bool producer_owner = !is_scene_worker
                && accepts_desktop_producer_message(wsh->connection_id());
            if (!is_scene_worker) {
                // Producer ownership must precede matrix_enabled. A new desktop
                // is legacy-compatible (producer=true until told otherwise),
                // while this ordering guarantees a standby client learns its
                // role before matrix_enabled can unlock its UDP stream.
                message.set_payload(DesktopControlProtocol::desktop_producer(producer_owner));
                wsh->send_message(message);
            }

            message.set_payload(DesktopControlProtocol::matrix_enabled(!config->is_turned_off()));
            wsh->send_message(message);

            send_initial_plugin_messages(wsh, producer_owner);

            return restinio::request_accepted();
        }

        spdlog::warn("WebSocket connection request rejected: Connection header is not upgrade.");
        return restinio::request_rejected(); });
    return std::move(router);
}

#include "live_frame_ws.h"

#include "live_frame_protocol.h"
#include "matrix_control/LiveFrameSnapshot.h"

#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include <restinio/websocket/websocket.hpp>
#include <spdlog/spdlog.h>

namespace {

struct LiveFrameClient {
    Server::rws::ws_handle_t socket;
    std::uint64_t requested_generation = 0;
};

std::mutex clients_mutex;
std::map<std::uint64_t, LiveFrameClient> clients;

void erase_client(const std::uint64_t connection_id)
{
    std::lock_guard lock(clients_mutex);
    clients.erase(connection_id);
}

void publish_to_waiting_clients(const LiveFrame::Snapshot &snapshot,
                                const std::uint64_t capture_generation)
{
    std::vector<std::pair<std::uint64_t, Server::rws::ws_handle_t>> waiting;
    {
        std::lock_guard lock(clients_mutex);
        for (auto &[connection_id, client] : clients) {
            if (client.requested_generation == 0 ||
                client.requested_generation > capture_generation)
                continue;

            client.requested_generation = 0;
            waiting.emplace_back(connection_id, client.socket);
        }
    }

    if (waiting.empty())
        return;

    const std::string payload = Server::LiveFrameProtocol::encode(snapshot);
    for (const auto &[connection_id, socket] : waiting) {
        try {
            Server::rws::message_t message;
            message.set_opcode(Server::rws::opcode_t::binary_frame);
            message.set_final_flag(Server::rws::final_frame_flag_t::final_frame);
            message.set_payload(payload);
            socket->send_message(std::move(message), [connection_id](const auto &error) {
                if (error)
                    erase_client(connection_id);
            });
        } catch (const std::exception &error) {
            spdlog::debug("Live frame WebSocket send failed: {}", error.what());
            erase_client(connection_id);
        }
    }
}

} // namespace

std::unique_ptr<Server::router_t> Server::add_live_frame_routes(std::unique_ptr<router_t> router)
{
    LiveFrame::SnapshotStore::instance().set_publish_callback(publish_to_waiting_clients);

    // /live_frame is intentionally WebSocket-only. The connection itself is
    // free from matrix-capture work; each text "next" asks for exactly one
    // future composed frame and duplicate requests from a waiting client are
    // ignored until that frame has been delivered.
    router->http_get("/live_frame", [](auto req, auto)
                     {
        if (restinio::http_connection_header_t::upgrade != req->header().connection())
            return req->create_response(restinio::status_bad_request())
                .append_header(restinio::http_field::content_type, "text/plain; charset=utf-8")
                .set_body("WebSocket upgrade required")
                .done();

        auto wsh = rws::upgrade<traits_t>(
            *req,
            rws::activation_t::delayed,
            [](auto socket, auto message) {
                const auto connection_id = socket->connection_id();
                if (rws::opcode_t::text_frame == message->opcode()) {
                    if (message->payload() != "next")
                        return;

                    std::lock_guard lock(clients_mutex);
                    const auto it = clients.find(connection_id);
                    if (it != clients.end() && it->second.requested_generation == 0)
                        it->second.requested_generation =
                            LiveFrame::SnapshotStore::instance().request_capture();
                    return;
                }

                if (rws::opcode_t::ping_frame == message->opcode()) {
                    auto pong = *message;
                    pong.set_opcode(rws::opcode_t::pong_frame);
                    socket->send_message(std::move(pong));
                    return;
                }

                if (rws::opcode_t::connection_close_frame == message->opcode())
                    erase_client(connection_id);
            });

        {
            std::lock_guard lock(clients_mutex);
            clients.emplace(wsh->connection_id(), LiveFrameClient{wsh, 0});
        }
        // Start reads only after the client is registered so an eager browser
        // cannot race its first `next` message against registry insertion.
        activate(*wsh);
        return restinio::request_accepted(); });

    return router;
}

void Server::clear_live_frame_connections()
{
    std::lock_guard lock(clients_mutex);
    clients.clear();
}

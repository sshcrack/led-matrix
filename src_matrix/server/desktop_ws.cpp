#include "desktop_ws.h"
#include <restinio/websocket/websocket.hpp>
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include <spdlog/spdlog.h>

std::unique_ptr<router_t> Server::add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t &registry)
{
  router->http_get("/desktopWebsocket", [&registry](auto req, auto)
                   {

                  spdlog::info("WebSocket connection request received.");
            if( restinio::http_connection_header_t::upgrade == req->header().connection() )
      {
        auto wsh =
          rws::upgrade< traits_t >(
            *req,
            rws::activation_t::immediate,
            [ &registry ]( auto wsh, auto m ){
              if( rws::opcode_t::ping_frame == m->opcode() )
							{
								auto resp = *m;
								resp.set_opcode( rws::opcode_t::pong_frame );
								wsh->send_message( resp );
							}
							else if( rws::opcode_t::connection_close_frame == m->opcode() )
							{
              std::unique_lock lock(registryMutex);
								registry.erase( wsh->connection_id() );
							}
            } );
        // Store websocket handle to registry object to prevent closing of the websocket
        // on exit from this request handler.

        std::unique_lock lock1(currSceneMutex);

        restinio::websocket::basic::message_t message;
        message.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
        message.set_payload(currScene);

        wsh->send_message(message);

        std::unique_lock lock(registryMutex);
        registry.emplace( wsh->connection_id(), wsh );

        spdlog::info("WebSocket connection established with ID: {}", wsh->connection_id());
        return restinio::request_accepted();
      }

      spdlog::warn("WebSocket connection request rejected: Connection header is not upgrade.");
      return restinio::request_rejected(); });
  return std::move(router);
}
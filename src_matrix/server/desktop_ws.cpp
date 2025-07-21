#include "desktop_ws.h"
#include <restinio/websocket/websocket.hpp>
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"


std::unique_ptr<router_t> Server::add_desktop_routes(std::unique_ptr<router_t> router, ws_registry_t & registry)
{
    router->http_get("/desktopWebsocket", [&registry](auto req, auto) mutable {
            if( restinio::http_connection_header_t::upgrade == req->header().connection() )
      {
        auto wsh =
          rws::upgrade< traits_t >(
            *req,
            rws::activation_t::immediate,
            [ &registry ]( auto wsh, auto m ){
              wsh->send_message( *m );
            } );
        // Store websocket handle to registry object to prevent closing of the websocket
        // on exit from this request handler.
        registry.emplace( wsh->connection_id(), wsh );

        return restinio::request_accepted();
      }

      return restinio::request_rejected();
    });
    return std::move(router);
}
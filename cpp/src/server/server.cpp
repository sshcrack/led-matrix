#include "restinio/all.hpp"
#include "server.h"
#include <expected>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

// Create request handler.
restinio::request_handling_status_t handler(const restinio::request_handle_t &req) {
    if (restinio::http_method_get() == req->header().method() &&
        req->header().request_target() == "/") {
        req->create_response()
                .append_header(restinio::http_field::server, "RESTinio hello world server")
                .append_header_date_field()
                .append_header(restinio::http_field::content_type, "text/plain; charset=utf-8")
                .set_body(
                        fmt::format(
                                RESTINIO_FMT_FORMAT_STRING("{}: Hello world!"),
                                restinio::fmtlib_tools::streamed(req->remote_endpoint())))
                .done();

        return restinio::request_accepted();
    }

    return restinio::request_rejected();
}


expected<tuple<server_t*, thread>, string> server_mainloop(uint16_t port) {
    try {
        server_t server{
                restinio::own_io_context(),
                restinio::server_settings_t<>{}
                        .port(port)
                        .address("localhost")
                        .request_handler(handler)
        };

        thread restinio_control_thread{[&server, &port] {
            // Use restinio::run to launch RESTinio's server.
            // This run() will return only if server stopped from
            // some other thread.
            info("Listening on http://localhost:{}/", port);
            restinio::run(restinio::on_thread_pool(
                    1, // Count of worker threads for RESTinio.
                    restinio::skip_break_signal_handling(), // Don't react to Ctrl+C.
                    server) // Server to be run.
            );
        }
        };

        //tuple<server_t*, thread> s = make_tuple(&server, std::move(restinio_control_thread));
        //return s;
        return unexpected("yees");
    }
    catch (const exception &ex) {
        return unexpected(ex.what());
    }
}
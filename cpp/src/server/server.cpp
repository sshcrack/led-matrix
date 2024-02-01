#include "restinio/all.hpp"
#include "server.h"
#include "../shared.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;

// Create request handler.
request_handling_status_t req_handler(const request_handle_t &req) {
    if (http_method_get() == req->header().method() &&
        req->header().request_target() == "/skip") {
        skip_image.store(true);
        req->create_response()
                .append_header_date_field()
                .append_header(http_field::content_type, "text/plain; charset=utf-8")
                .set_body("Skipping img")
                .done();

        return request_accepted();
    }

    return request_rejected();
}
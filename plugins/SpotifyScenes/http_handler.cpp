#include "http_handler.h"

std::optional<restinio::request_handling_status_t> handle_request(const restinio::request_handle_t &req) {
    const auto target = req->header().path();

    if (target == "/spotify/login") {

    }

}

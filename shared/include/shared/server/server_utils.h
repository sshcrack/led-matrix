#pragma once

#include "restinio/all.hpp"
#include "nlohmann/json.hpp"
#include <string>

namespace Server {
    using namespace restinio;
    using json = nlohmann::json;

    restinio::request_handling_status_t reply_with_json(const request_handle_t &req, const json &j,
                                                        http_status_line_t status = status_ok());

    request_handling_status_t reply_with_error(const request_handle_t &req, std::string msg,
                                               http_status_line_t status = status_bad_request());

    restinio::request_handling_status_t reply_success(const request_handle_t &req);
}
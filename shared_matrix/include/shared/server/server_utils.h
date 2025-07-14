#pragma once

#include "restinio/all.hpp"
#include "nlohmann/json.hpp"
#include <string>

namespace Server {
    using namespace restinio;
    using json = nlohmann::json;

    // Helper function to add CORS headers if enabled
    template<typename ResponseBuilder>
    ResponseBuilder& add_cors_headers(ResponseBuilder& response_builder) {
#ifdef ENABLE_CORS
        response_builder.append_header("Access-Control-Allow-Origin", "*");
        response_builder.append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        response_builder.append_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        response_builder.append_header("Access-Control-Max-Age", "86400"); // 24 hours
#endif
        return response_builder;
    }

    restinio::request_handling_status_t reply_with_json(const request_handle_t &req, const json &j,
                                                        http_status_line_t status = status_ok());

    request_handling_status_t reply_with_error(const request_handle_t &req, std::string msg,
                                               http_status_line_t status = status_bad_request());

    restinio::request_handling_status_t reply_success(const request_handle_t &req);

    // Handle CORS preflight requests
    restinio::request_handling_status_t handle_cors_preflight(const request_handle_t &req);
}
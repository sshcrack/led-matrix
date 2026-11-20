#pragma once
#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include <optional>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"
#include "restinio/all.hpp"

namespace Server {
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

    restinio::request_handling_status_t reply_with_json(const restinio::request_handle_t &req, const json &j,
                                                        restinio::http_status_line_t status = restinio::status_ok());

    restinio::request_handling_status_t reply_with_error(const restinio::request_handle_t &req, std::string msg,
                                               restinio::http_status_line_t status = restinio::status_bad_request());

    restinio::request_handling_status_t reply_success(const restinio::request_handle_t &req);

    /// Answer to GET on a route that changes state (docs/API.md): 405 with
    /// `Allow: POST`, so old clients learn the new method instead of changing
    /// anything.
    restinio::request_handling_status_t reply_use_post(const restinio::request_handle_t& req);

    /// A request parameter from the query string, a JSON object body
    /// ({"name": "value"} or {"name": true}) or a form body (name=value), in
    /// that order. Booleans become "true"/"false".
    std::optional<std::string> request_param(const restinio::request_handle_t& req, std::string_view name);

    // Handle CORS preflight requests
    restinio::request_handling_status_t handle_cors_preflight(const restinio::request_handle_t &req);

    [[nodiscard]] bool is_desktop_connected();
}
#include "shared/server/server_utils.h"
#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include "nlohmann/json.hpp"

namespace Server {
    using namespace std;
    using namespace spdlog;
    using namespace restinio;
    using json = nlohmann::json;

    request_handling_status_t reply_with_json(const request_handle_t &req, const json &j,
                                                        http_status_line_t status) {
        auto response = req->create_response(std::move(status))
                .append_header_date_field()
                .append_header(http_field::content_type, "application/json; charset=utf-8");
        
        add_cors_headers(response);
        
        return response.set_body(to_string(j)).done();
    }

    request_handling_status_t reply_with_error(const request_handle_t &req, const string msg, http_status_line_t status) {
        return reply_with_json(req, json{{"error", msg}}, std::move(status));
    }

    request_handling_status_t reply_success(const request_handle_t &req) {
        return reply_with_json(req, json{{"success", true}});
    }

    request_handling_status_t handle_cors_preflight(const request_handle_t &req) {
#ifdef ENABLE_CORS
        auto response = req->create_response(status_ok())
                .append_header_date_field();
        
        add_cors_headers(response);
        
        return response.done();
#else
        return req->create_response(status_method_not_allowed()).done();
#endif
    }
}

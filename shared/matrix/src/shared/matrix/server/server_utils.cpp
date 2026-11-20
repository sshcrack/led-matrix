#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/server/common.h"
#include "restinio/all.hpp"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
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

    request_handling_status_t reply_use_post(const request_handle_t& req)
    {
        auto response = req->create_response(status_method_not_allowed())
                            .append_header_date_field()
                            .append_header(http_field::allow, "POST")
                            .append_header(http_field::content_type, "application/json; charset=utf-8");
        add_cors_headers(response);
        const std::string path{req->header().path()};
        return response.set_body(to_string(json{{"error", "This changes the matrix: use POST " + path}})).done();
    }

    std::optional<std::string> request_param(const request_handle_t& req, std::string_view name)
    {
        const auto query = parse_query(req->header().query());
        if (query.has(name))
            return std::string{query[name]};
        const auto& body = req->body();
        if (body.empty())
            return std::nullopt;
        if (const auto j = json::parse(body, nullptr, false); j.is_object()) {
            const auto it = j.find(std::string{name});
            if (it == j.end())
                return std::nullopt;
            if (it->is_string())
                return it->get<std::string>();
            if (it->is_boolean())
                return it->get<bool>() ? "true" : "false";
            if (it->is_number())
                return it->dump();
            return std::nullopt;
        }
        try {
            const auto form = parse_query(body);
            if (form.has(name))
                return std::string{form[name]};
        }
        catch (const std::exception&) {
        }
        return std::nullopt;
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

    [[nodiscard]] bool is_desktop_connected() {
        return desktop_connection_count > 0;
    }
}

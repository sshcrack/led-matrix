#include "restinio/all.hpp"
#include "server.h"
#include "../shared.h"
#include "nlohmann/json.hpp"
#include <spdlog/spdlog.h>

#include <utility>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

void reply_with_json(const request_handle_t &req, const json j, http_status_line_t status = status_ok()) {
    req->create_response(std::move(status))
            .append_header_date_field()
            .append_header(http_field::content_type, "application/data; charset=utf-8")
            .set_body(to_string(j));
}

void reply_with_error(const request_handle_t &req, const string msg, http_status_line_t status = status_bad_request()) {
    reply_with_json(req, json{{"error", msg}}, std::move(status));
}

void reply_success(const request_handle_t &req) {
    reply_with_json(req, json{{"success", true}});
}

// Create request handler.
request_handling_status_t req_handler(const request_handle_t &req) {
    if (http_method_get() != req->header().method())
        return request_rejected();

    auto target = req->header().request_target();
    const auto qp = restinio::parse_query(req->header().query());

    if (target == "/skip") {
        skip_image.store(true);
        reply_success(req);

        return request_accepted();
    }

    if (target == "/preset") {
        if (!qp.has("id")) {
            reply_with_error(req, "No Id given");
            return request_accepted();
        }

        string id { qp["id"] };
        auto groups = config->get_groups();
        if (groups.find(id) == groups.end()) {
            reply_with_error(req, "Invalid id");
            return request_accepted();
        }

        config->setCurr(id);
    }

    return request_rejected();
}
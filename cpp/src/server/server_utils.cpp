#include "server_utils.h"
#include "restinio/all.hpp"
#include "../utils/shared.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "../utils/utils.h"
#include "../utils/consts.h"

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

void reply_with_json(const request_handle_t &req, const json &j, http_status_line_t status) {
    req->create_response(std::move(status))
            .append_header_date_field()
            .append_header(http_field::content_type, "application/json; charset=utf-8")
            .set_body(to_string(j))
            .done();
}

void reply_with_error(const request_handle_t &req, const string msg, http_status_line_t status) {
    reply_with_json(req, json{{"error", msg}}, std::move(status));
}

void reply_success(const request_handle_t &req) {
    reply_with_json(req, json{{"success", true}});
}
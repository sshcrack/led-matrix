#include "restinio/all.hpp"
#include "../shared.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "../utils.h"
#include "server_utils.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

request_handling_status_t handle_post(const request_handle_t &req) {
    string target(req->header().path());

    if(target == "/add_preset") {
        debug("Adding preset...");
        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
        } catch (exception& ex) {
            warn("Invalid json payload {}", ex.what());

            reply_with_error(req, "Invalid json payload");
            return request_accepted();
        }

        ConfigData::Preset pr;
        try {
            pr = j.template get<ConfigData::Preset>();
        } catch (exception& ex) {
            warn("Invalid preset");

            reply_with_error(req, "Could not serialize json");
            return request_accepted();
        }
    }
}
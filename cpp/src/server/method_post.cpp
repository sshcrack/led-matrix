#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "shared/utils/uuid.h"
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
            std::string uuid = uuid::generate_uuid_v4();

            config->set_presets(uuid, pr);
            json return_j;
            return_j["success"] = "Preset has been added";
            return_j["id"] = uuid;

            reply_with_json(req, return_j);
        } catch (exception& ex) {
            warn("Invalid preset with {}", ex.what());

            reply_with_error(req, "Could not serialize json");
        }
        return request_accepted();
    }

    return request_rejected();
}
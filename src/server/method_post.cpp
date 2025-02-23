#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "shared/utils/uuid.h"
#include "shared/server/server_utils.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using namespace restinio;
using json = nlohmann::json;

request_handling_status_t handle_post(const request_handle_t &req) {
    const auto target(req->header().path());
    const auto qp = parse_query(req->header().query());

    if (target == "/add_preset") {
        if (!qp.has("id")) {
            reply_with_error(req, "Id not given");
            return request_accepted();
        }
        std::string id{qp["id"]};
        if (id == "") {
            reply_with_error(req, "Id empty");
            return request_accepted();
        }


        debug("Adding preset...");
        string str_body = req->body();
        json j;
        try {
            j = json::parse(str_body);
        } catch (exception &ex) {
            warn("Invalid json payload {}", ex.what());

            reply_with_error(req, "Invalid json payload");
            return request_accepted();
        }

        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset> >();

            config->set_presets(id, pr);
            json return_j;
            return_j["success"] = "Preset has been added";

            reply_with_json(req, return_j);
        } catch (exception &ex) {
            warn("Invalid preset with {}", ex.what());

            reply_with_error(req, "Could not serialize json");
        }
        return request_accepted();
    }

    if (target == "/preset") {
        if (!qp.has("id")) {
            reply_with_error(req, "Id not given");
            return request_accepted();
        }

        std::string id{qp["id"]};

        string str_body = req->body();
        json j;

        try {
            j = json::parse(str_body);
        } catch (exception &ex) {
            warn("Invalid json payload {}", ex.what());

            reply_with_error(req, "Invalid json payload");
            return request_accepted();
        }


        try {
            const auto pr = j.get<std::shared_ptr<ConfigData::Preset> >();

            config->set_presets(id, pr);
            json return_j;

            return_j["success"] = "Preset has been set";
            return_j["id"] = id;

            reply_with_json(req, return_j);
        } catch (exception &ex) {
            warn("Invalid preset with {}", ex.what());

            reply_with_error(req, "Could not serialize json");
        }
        return request_accepted();

        return request_accepted();
    }

    return request_rejected();
}

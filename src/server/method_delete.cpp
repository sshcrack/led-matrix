#include "method_delete.h"

#include <shared/server/server_utils.h>
#include <shared/utils/shared.h>

restinio::request_handling_status_t handle_delete(const restinio::request_handle_t &req) {
    const auto target = req->header().path();
    const auto qp = restinio::parse_query(req->header().query());

    if (target != "/preset")
        return restinio::request_rejected();

    if (!qp.has("id")) {
        reply_with_error(req, "Id not given");
        return restinio::request_accepted();
    }

    const std::string id{qp["id"]};
    if (config->get_curr_id() == id) {
        reply_with_error(req, "Can not delete current preset");
        return restinio::request_accepted();
    }

    config->delete_preset(id);

    reply_with_json(req, {{"success", "Preset has been deleted"}});
    return restinio::request_accepted();
}

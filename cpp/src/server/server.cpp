#include "server.h"
#include "restinio/all.hpp"
#include "shared/utils/shared.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include "method_post.h"
#include "method_get.h"
#include "shared/plugin_loader/loader.h"

using namespace std;
using namespace restinio;
using json = nlohmann::json;


// Create request handler.
request_handling_status_t req_handler(const request_handle_t &req) {
    auto pl = Plugins::PluginManager::instance();
    for (const auto &item: pl->get_plugins()) {
        auto to_return = item->handle_request(req);
        if (to_return.has_value()) {
            return to_return.value();
        }
    }

    if (http_method_post() == req->header().method())
        return handle_post(req);

    if (http_method_get() == req->header().method())
        return handle_get(req);

    return request_rejected();
}
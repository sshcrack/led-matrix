#include "restinio/all.hpp"
#include "server.h"
#include "../shared.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include "../utils.h"
#include "post.h"
#include "get.h"

using namespace std;
using namespace restinio;
using json = nlohmann::json;


// Create request handler.
request_handling_status_t req_handler(const request_handle_t &req) {
    if(http_method_post() != req->header().method())
        return handle_post(req);

    if(http_method_post() != req->header().method())
        return handle_get(req);

    return request_rejected();
}
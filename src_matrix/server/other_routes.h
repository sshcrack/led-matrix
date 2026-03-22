#pragma once
#include "restinio/all.hpp"

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    using namespace std;

    std::unique_ptr<router_t> add_other_routes(std::unique_ptr<router_t> router);
    restinio::request_handling_status_t handle_web_request(const restinio::request_handle_t &req, const restinio::string_view_t requested_path);
}
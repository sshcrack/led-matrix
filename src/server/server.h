#pragma once
#include <restinio/core.hpp>

namespace Server {
    using router_t = restinio::router::express_router_t<>;
    using namespace std;

    std::unique_ptr<router_t> server_handler();

}
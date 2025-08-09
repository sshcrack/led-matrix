#pragma once

#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/update/UpdateManager.h"
#include <restinio/core.hpp>

namespace Server {
    std::unique_ptr<router_t> add_update_routes(std::unique_ptr<router_t> router, Update::UpdateManager* update_manager);
}
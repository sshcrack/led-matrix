#pragma once

#include "shared/matrix/server/common.h"

namespace Server {
    std::unique_ptr<router_t> add_post_processing_routes(std::unique_ptr<router_t> router);
}
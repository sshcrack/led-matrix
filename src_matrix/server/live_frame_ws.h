#pragma once

#include "shared/matrix/server/common.h"

namespace Server {

std::unique_ptr<router_t> add_live_frame_routes(std::unique_ptr<router_t> router);
void clear_live_frame_connections();

} // namespace Server

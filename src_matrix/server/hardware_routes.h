#pragma once
#include "server.h"

namespace Server {
/// GET/POST /hardware: panel geometry and timing (MATRIX_OPTS), applied by the
/// root helper scripts/apply_hardware.sh through led-matrix-hardware.path.
std::unique_ptr<router_t> add_hardware_routes(std::unique_ptr<router_t> router);
}  // namespace Server

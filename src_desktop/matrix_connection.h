#pragma once

#include <cstdint>
#include <string>

namespace desktop_app::detail {
    /// Turns the matrix on/off through its REST API (synchronous with timeout when
    /// waitForCompletion, otherwise on a detached thread).
    void change_matrix_status(const std::string &hostname, uint16_t port, bool turnOn, bool waitForCompletion = false);

    /// Creates the WebsocketClient and registers it as the global instance.
    void init_websocket();
}

#pragma once

#include <shared/common/desktop_control_protocol.h>

#include <atomic>
#include <string_view>

/// Tracks whether bulk desktop->matrix UDP traffic is currently useful.
///
/// A WebSocket connection alone is not enough: every fresh connection must
/// receive an explicit matrix-enabled status before UDP is allowed. This keeps
/// autostarted desktop clients quiet while the matrix is off or unreachable and
/// avoids reusing stale state across reconnects.
class DesktopStreamState final {
public:
    void on_websocket_open()
    {
        websocketOpen_.store(true, std::memory_order_release);
        matrixEnabled_.store(false, std::memory_order_release);
    }

    void on_websocket_closed()
    {
        matrixEnabled_.store(false, std::memory_order_release);
        websocketOpen_.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool on_control_message(std::string_view message)
    {
        const auto enabled = DesktopControlProtocol::parse_matrix_enabled(message);
        if (!enabled.has_value())
            return false;

        matrixEnabled_.store(*enabled, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool can_send_udp() const
    {
        return websocketOpen_.load(std::memory_order_acquire) && matrixEnabled_.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> websocketOpen_{false};
    std::atomic<bool> matrixEnabled_{false};
};

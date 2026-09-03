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
        // Legacy matrices do not know about producer ownership. Assume this
        // desktop owns the stream until a newer matrix explicitly tells us
        // otherwise. New matrices send desktop_producer:* before
        // matrix_enabled:*, so there is no window where a standby desktop can
        // start sending UDP during the handshake.
        producerOwner_.store(true, std::memory_order_release);
    }

    void on_websocket_closed()
    {
        matrixEnabled_.store(false, std::memory_order_release);
        producerOwner_.store(true, std::memory_order_release);
        websocketOpen_.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool on_control_message(std::string_view message)
    {
        if (const auto enabled = DesktopControlProtocol::parse_matrix_enabled(message); enabled.has_value()) {
            matrixEnabled_.store(*enabled, std::memory_order_release);
            return true;
        }
        if (const auto owner = DesktopControlProtocol::parse_desktop_producer(message); owner.has_value()) {
            producerOwner_.store(*owner, std::memory_order_release);
            return true;
        }
        return false;
    }

    [[nodiscard]] bool can_send_udp() const
    {
        return websocketOpen_.load(std::memory_order_acquire)
            && matrixEnabled_.load(std::memory_order_acquire)
            && producerOwner_.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> websocketOpen_{false};
    std::atomic<bool> matrixEnabled_{false};
    std::atomic<bool> producerOwner_{false};
};

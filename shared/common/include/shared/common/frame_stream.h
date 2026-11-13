#pragma once

#include <shared/common/macro.h>
#include <shared/common/udp/packet.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

/// Desktop-rendered frames that a matrix scene shows as they are (shader,
/// video and music-video streams). The desktop sends each RGB888 frame as
/// sequenced chunks in the RemoteRender chunk layout, with the stream's own
/// packet id, and the matrix keeps the newest complete frame.
///
/// Older matrices only understand one whole frame per datagram. The matrix
/// plugin announces `CapabilityMessage` when its desktop plugin connects;
/// until then the desktop keeps sending the legacy datagram.
namespace FrameStream {

/// First header byte of a chunk (RemoteRender uses its protocol version there).
inline constexpr std::uint8_t HeaderVersion = 1;
/// Sent by the matrix plugin to its desktop plugin on every connection.
inline constexpr std::string_view CapabilityMessage = "frame_stream:1";

/// Desktop side of one stream. Thread-safe: messages arrive on the WebSocket
/// thread, frames are packed on the UDP sender thread.
class SHARED_COMMON_API Sender {
public:
    using LegacyPacket = std::function<std::unique_ptr<UdpPacket>(std::vector<std::uint8_t>)>;

    explicit Sender(std::uint8_t packet_id);

    /// A new connection: send legacy datagrams until the matrix announces chunks.
    void reset();
    /// True when `message` was the matrix's capability announcement (consumed).
    bool on_message(std::string_view message);
    [[nodiscard]] bool chunked() const { return chunked_.load(std::memory_order_acquire); }

    /// Packets for one top-down RGB888 frame: chunks when the matrix accepts
    /// them and the size matches `width`x`height`, else `legacy(rgb)`.
    std::vector<std::unique_ptr<UdpPacket>> packets(std::vector<std::uint8_t> rgb, int width, int height, const LegacyPacket& legacy);
    /// Chunks of one top-down frame regardless of the capability; empty when
    /// the size does not match. For callers that prepare chunked frames differently.
    std::vector<std::unique_ptr<UdpPacket>> chunk_packets(std::span<const std::uint8_t> rgb, int width, int height);

private:
    std::uint8_t packet_id_;
    std::atomic<bool> chunked_{false};
    /// Increases across desktop restarts, so the matrix can drop chunks of an
    /// older run that arrive late.
    std::uint32_t session_;
    std::uint32_t sequence_ = 0;
};

}  // namespace FrameStream

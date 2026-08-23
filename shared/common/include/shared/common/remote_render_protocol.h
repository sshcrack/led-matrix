#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <shared/common/macro.h>
#include <shared/common/udp/packet.h>

namespace RemoteRenderProtocol {

inline constexpr std::uint8_t PluginId = 0x05;
inline constexpr std::uint8_t Version = 2;
inline constexpr std::size_t HeaderBytes = 21;
inline constexpr std::size_t ChunkBytes = 1200;

class SHARED_COMMON_API FramePacket final : public UdpPacket {
public:
    FramePacket(std::uint32_t session, std::uint32_t sequence,
                std::uint16_t width, std::uint16_t height,
                std::uint16_t chunk_index, std::uint16_t chunk_count,
                std::uint32_t byte_offset,
                std::vector<std::uint8_t> chunk);

    [[nodiscard]] std::vector<std::uint8_t> toData() const override;

private:
    std::uint32_t session_;
    std::uint32_t sequence_;
    std::uint16_t width_;
    std::uint16_t height_;
    std::uint16_t chunk_index_;
    std::uint16_t chunk_count_;
    std::uint32_t byte_offset_;
    std::vector<std::uint8_t> chunk_;
};

/// Split one RGB888 frame into MTU-safe datagrams. The matrix receiver keeps
/// only the newest complete frame, so packet loss costs freshness rather than
/// building a latency queue.
SHARED_COMMON_API std::vector<std::unique_ptr<UdpPacket>> make_frame_packets(
    std::uint32_t session, std::uint32_t sequence,
    std::uint16_t width, std::uint16_t height,
    std::span<const std::uint8_t> rgb);

} // namespace RemoteRenderProtocol

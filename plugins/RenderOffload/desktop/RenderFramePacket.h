#pragma once

#include <cstdint>
#include <vector>

#include <shared/common/udp/packet.h>

class RenderFramePacket final : public UdpPacket {
public:
    RenderFramePacket(std::uint32_t session, std::uint32_t sequence,
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

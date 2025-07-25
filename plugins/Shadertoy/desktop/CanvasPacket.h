#pragma once
#include <shared/common/udp/packet.h>


struct CanvasPacket final : UdpPacket {
    std::vector<uint8_t> data;

public:
    CanvasPacket(std::vector<uint8_t> rgbData);
    std::vector<uint8_t> toData() const override;
};

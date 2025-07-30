#pragma once
#include <shared/common/udp/packet.h>

struct CompactAudioPacket final : UdpPacket
{
public:
    CompactAudioPacket(const std::vector<float> &bands, bool interpolatedLog);
    std::vector<uint8_t> toData() const override;
private:
    uint32_t timestamp;
    uint8_t numBands;

    std::vector<uint8_t> bands;
    uint8_t flags;
};
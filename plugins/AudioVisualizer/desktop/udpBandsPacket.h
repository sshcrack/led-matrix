#pragma once

#include <shared/common/audio_protocol.h>
#include <shared/common/udp/packet.h>

struct MusicAnalysisPacket final : UdpPacket {
    explicit MusicAnalysisPacket(AudioProtocol::Frame frame);
    std::vector<uint8_t> toData() const override;

private:
    AudioProtocol::Frame frame_;
};

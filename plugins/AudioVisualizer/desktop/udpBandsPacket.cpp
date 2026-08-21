#include "udpBandsPacket.h"
#include <utility>

MusicAnalysisPacket::MusicAnalysisPacket(AudioProtocol::Frame frame)
    : UdpPacket(0x01), frame_(std::move(frame)) {}

std::vector<uint8_t> MusicAnalysisPacket::toData() const {
    return AudioProtocol::encode(frame_);
}

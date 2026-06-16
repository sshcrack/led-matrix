#include "SpotifyMVPacket.h"

SpotifyMVPacket::SpotifyMVPacket(std::vector<uint8_t> rgbData)
    : UdpPacket(0x04), data(std::move(rgbData)) {}

std::vector<uint8_t> SpotifyMVPacket::toData() const { return data; }

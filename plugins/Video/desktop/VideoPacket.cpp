#include "VideoPacket.h"

VideoPacket::VideoPacket(std::vector<uint8_t> rgbData)
    : UdpPacket(0x03), data(std::move(rgbData)) {}

std::vector<uint8_t> VideoPacket::toData() const { return data; }

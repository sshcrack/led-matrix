#include "VideoPacket.h"

VideoPacket::VideoPacket(std::vector<uint8_t> rgbData)
    : UdpPacket(0), data(std::move(rgbData)) {}

std::vector<uint8_t> VideoPacket::toData() const { return data; }

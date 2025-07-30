#include "CanvasPacket.h"

CanvasPacket::CanvasPacket(std::vector<uint8_t> rgbData)
    : UdpPacket(0x02), data(std::move(rgbData)) {
}

std::vector<uint8_t> CanvasPacket::toData() const {
    return data;
}

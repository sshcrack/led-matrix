#pragma once
#include <shared/common/udp/packet.h>
#include <vector>

struct VideoPacket final : UdpPacket {
  std::vector<uint8_t> data;

public:
  VideoPacket(std::vector<uint8_t> rgbData);
  std::vector<uint8_t> toData() const override;
};

#pragma once
#include <shared/common/udp/packet.h>
#include <vector>

struct SpotifyMVPacket final : UdpPacket {
  std::vector<uint8_t> data;

  explicit SpotifyMVPacket(std::vector<uint8_t> rgbData);
  std::vector<uint8_t> toData() const override;
};

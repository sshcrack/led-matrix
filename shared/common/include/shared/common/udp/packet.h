#pragma once
#include <cstdint>
#include <vector>

struct UdpPacket {
    virtual ~UdpPacket() = default;

    uint8_t pluginId;

    UdpPacket(const uint8_t plId) : pluginId(plId) {
    }

    [[nodiscard]] virtual std::vector<uint8_t> toData() const = 0;

    [[nodiscard]] std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> data = toData();

        std::vector<uint8_t> packet;
        packet.push_back(0xAD); // Magic byte
        packet.push_back(0x01); // Magic byte
        packet.push_back(pluginId); // Version byte

        uint32_t payload_size = static_cast<uint32_t>(data.size());
        // Insert size as 4 bytes (big-endian / network order)
        packet.push_back(static_cast<uint8_t>((payload_size >> 24) & 0xFF));
        packet.push_back(static_cast<uint8_t>((payload_size >> 16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((payload_size >> 8) & 0xFF));
        packet.push_back(static_cast<uint8_t>(payload_size & 0xFF));
        packet.insert(packet.end(), data.begin(), data.end());
        return packet;
    }
};

#pragma once
#include <cstdint>
#include <vector>

struct UdpPacket {
    virtual ~UdpPacket() = default;

    uint8_t magic;
    uint8_t version;

    UdpPacket(const uint8_t magic, const uint8_t version) : magic(magic), version(version) {
    }

    [[nodiscard]] virtual std::vector<uint8_t> toData() const = 0;

    [[nodiscard]] std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> data = toData();
        data.insert(data.begin(), 0xAD); // Magic byte
        data.insert(data.begin() + 1, magic); // Magic byte

        data.insert(data.begin() + 2, version); // Version byte

        return data;
    }
};

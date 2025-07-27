#pragma once
#include <cstdint>
#include <vector>
#include "shared/common/utils/macro.h"

struct SHARED_COMMON_API UdpPacket {
    virtual ~UdpPacket() = default;

    uint8_t pluginId;

    explicit UdpPacket(const uint8_t plId) : pluginId(plId) {
    }

    [[nodiscard]] virtual std::vector<uint8_t> toData() const = 0;

    [[nodiscard]] std::vector<uint8_t> toBytes() const;
};

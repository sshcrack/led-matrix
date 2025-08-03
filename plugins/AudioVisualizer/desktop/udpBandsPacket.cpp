#include "udpBandsPacket.h"
#include <chrono>
#include <algorithm>
#include <cstring>

CompactAudioPacket::CompactAudioPacket(const std::vector<float> &bands, bool interpolatedLog, bool beatDetected) : UdpPacket(0x01)
{

    numBands = static_cast<uint8_t>(std::min(bands.size(), static_cast<size_t>(255)));
    timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());

    // Set flags based on configuration
    // Bit 0: interpolatedLog
    // Bit 1: beatDetected
    flags = (interpolatedLog ? 1 : 0) | (beatDetected ? 2 : 0);

    // Convert float bands to uint8_t (0-255) for compactness
    this->bands.reserve(numBands);

    for (size_t i = 0; i < numBands; ++i)
    {
        this->bands.push_back(static_cast<uint8_t>(std::min(bands[i] * 255.0f, 255.0f)));
    }
}

std::vector<uint8_t> CompactAudioPacket::toData() const
{
    std::vector<uint8_t> data;

    data.reserve(6 + bands.size());

    // Header (6 bytes total)

    data.push_back(numBands); // 1 byte
    data.push_back(flags);    // 1 byte

    uint8_t timestampBytes[4];
    std::memcpy(timestampBytes, &timestamp, 4);

    data.insert(data.end(), timestampBytes, timestampBytes + 4); // 4 bytes

    // Band data (numBands bytes)
    data.insert(data.end(), bands.begin(), bands.end());

    return data;
}

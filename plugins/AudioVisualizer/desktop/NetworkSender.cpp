#define NOMINMAX
#include "NetworkSender.h"

#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <expected>
#include <stdexcept>

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

CompactAudioPacket::CompactAudioPacket(const std::vector<float> &bands, bool interpolatedLog) {
    numBands = static_cast<uint8_t>(std::min(bands.size(), static_cast<size_t>(255)));
    timestamp = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());

    // Set flags based on configuration
    flags = interpolatedLog ? 1 : 0;

    // Convert float bands to uint8_t (0-255) for compactness
    this->bands.reserve(numBands);
    for (size_t i = 0; i < numBands; ++i) {
        this->bands.push_back(static_cast<uint8_t>(std::min(bands[i] * 255.0f, 255.0f)));
    }

    magic[0] = 0xAD;
    magic[1] = 0x01;
    version = 0x01;
}

[[nodiscard]] std::vector<uint8_t> CompactAudioPacket::toBytes() const {
    std::vector<uint8_t> packet;
    packet.reserve(9 + bands.size());

    // Header (9 bytes total)
    packet.insert(packet.end(), magic, magic + 2); // 2 bytes
    packet.push_back(version); // 1 byte
    packet.push_back(numBands); // 1 byte
    packet.push_back(flags); // 1 byte

    uint8_t timestampBytes[4];
    std::memcpy(timestampBytes, &timestamp, 4);
    packet.insert(packet.end(), timestampBytes, timestampBytes + 4); // 4 bytes

    // Band data (numBands bytes)
    packet.insert(packet.end(), bands.begin(), bands.end());

    return packet;
}

UdpSender::UdpSender() {
#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("Failed to create socket");
    }
#else
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }
#endif
}

UdpSender::~UdpSender() {
#if defined(_WIN32)
    closesocket(socket);
    WSACleanup();
#else
    close(socket);
#endif
}

std::expected<void, std::string> UdpSender::sendAudioData(const std::vector<float> &bands,
                                                          const std::string &targetAddr, uint16_t port,
                                                          bool interpolatedLog) {
    const CompactAudioPacket packet(bands, interpolatedLog);
    const std::vector<uint8_t> data = packet.toBytes();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // Use the provided port

#if defined(_WIN32)
    // Use InetPton or inet_pton if available
    const std::wstring wTargetAddr(targetAddr.begin(), targetAddr.end());
    const PCWSTR pcwStrTarget = wTargetAddr.c_str();

    if (InetPtonW(AF_INET, pcwStrTarget, &addr.sin_addr) != 1) {
        return std::unexpected("Invalid address: " + targetAddr);
    }
#else
        if (inet_pton(AF_INET, targetAddr.c_str(), &addr.sin_addr) != 1)
        {
            return std::unexpected("Invalid address: " + targetAddr);
        }
#endif

    int result = sendto(socket, reinterpret_cast<const char *>(data.data()), data.size(), 0,
                        reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

#if defined(_WIN32)
    if (result == SOCKET_ERROR) {
        return std::unexpected("Failed to send data with code: " + std::to_string(WSAGetLastError()));
    }
#else
    if (result < 0)
    {
        return std::unexpected("Failed to send data: " + std::string(strerror(errno)));
    }
#endif

    return {};
}

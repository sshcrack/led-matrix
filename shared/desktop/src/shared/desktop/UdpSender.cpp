//
// Created by hendrik on 7/21/25.
//

#include "shared/desktop/UdpSender.h"

#include <cstring>
#include "shared/desktop/plugin_loader/loader.h"

#ifdef _WIN32
#pragma comment(lib, "bcrypt.lib")
#include <ws2tcpip.h>
#endif

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

std::expected<void, std::string> UdpSender::sendPacket(std::unique_ptr<UdpPacket, void(*)(UdpPacket *)> packet, const std::string &targetAddr,
                                                       const uint16_t port) const {
    const std::vector<uint8_t> data = packet->toBytes();

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
#ifdef _WIN32
    auto toSend = reinterpret_cast<const char *>(data.data());
#else
    auto toSend = data.data();
#endif

    int result = sendto(socket, toSend, data.size(), 0,
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

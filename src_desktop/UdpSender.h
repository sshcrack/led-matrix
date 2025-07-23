//
// Created by hendrik on 7/21/25.
//

#pragma once
#define NOMINMAX
#include <complex.h>
#include <vector>
#include <string>
#include <expected>
#include <cstdint>
#include <shared/common/udp/packet.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <memory>

class UdpSender final
{
#if defined(_WIN32)
    SOCKET socket;
#else
    int socket;
#endif

public:
    UdpSender();

    ~UdpSender();
    [[nodiscard]] std::expected<void, std::string> sendPacket(std::unique_ptr<UdpPacket, void(*)(UdpPacket *)> packet, const std::string &targetAddr,
                                                              uint16_t port) const;
};

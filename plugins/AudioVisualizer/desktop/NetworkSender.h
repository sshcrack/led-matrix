#pragma once

#define NOMINMAX
#include <vector>
#include <string>
#include <expected>
#include <cstdint>
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

struct CompactAudioPacket {
    uint8_t magic[2]{};
    uint8_t version;
    uint8_t numBands;
    uint8_t flags;
    uint32_t timestamp;
    std::vector<uint8_t> bands;

    public:
        CompactAudioPacket(const std::vector<float>& bands, bool interpolatedLog);
        [[nodiscard]] std::vector<uint8_t> toBytes() const;
};

class NetworkSender {
public:
    virtual std::expected<void, std::string> sendAudioData(const std::vector<float> &bands,
                                                           const std::string &targetAddr, uint16_t port,
                                                           bool interpolatedLog) = 0;
    virtual ~NetworkSender() = default;
};

class UdpSender final : public NetworkSender {
#if defined(_WIN32)
    SOCKET socket;
#else
    int socket;
#endif

public:
    UdpSender();
    ~UdpSender() override;

    std::expected<void, std::string> sendAudioData(const std::vector<float> &bands, const std::string &targetAddr,
                                                   uint16_t port, bool interpolatedLog) override;
};

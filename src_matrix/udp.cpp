#include "udp.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <shared/matrix/plugin_loader/loader.h>

void UdpServer::server_loop()
{
    constexpr size_t buffer_size = 64 * 1024; // Larger buffer for big packets
    std::vector<uint8_t> receive_buffer(buffer_size);
    std::vector<uint8_t> packet_buffer; // For reassembling large packets
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    while (server_running)
    {
        ssize_t n = recvfrom(udp_socket, receive_buffer.data(), receive_buffer.size(), 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No data available, sleep briefly to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            else
            {
                spdlog::error("UDP recvfrom error: {}", strerror(errno));
                break;
            }
        }

        // Append received data to packet buffer
        packet_buffer.insert(packet_buffer.end(), receive_buffer.begin(), receive_buffer.begin() + n);

        // Process complete packets in the buffer
        size_t offset = 0;
        while (packet_buffer.size() - offset >= 7)
        {
            const uint8_t *data = packet_buffer.data() + offset;

            // Check magic number (2 bytes: 0xAD, 0x01)
            if (data[0] != 0xAD || data[1] != 0x01)
            {
                // Invalid packet, skip one byte and try again
                offset += 1;
                continue;
            }

            const uint8_t pluginId = data[2];

            // Parse payload size (4 bytes, network byte order)
            uint32_t payload_size = (static_cast<uint32_t>(data[3]) << 24) |
                                    (static_cast<uint32_t>(data[4]) << 16) |
                                    (static_cast<uint32_t>(data[5]) << 8) |
                                    (static_cast<uint32_t>(data[6]));

            // Check if we have the complete packet
            if (packet_buffer.size() - offset < 7 + payload_size)
            {
                // Not enough data for full payload, wait for more data
                break;
            }

            const uint8_t *payload = data + 7;

            // Pass to plugins (note: using data[1] as magicPacket for backward compatibility)
            for (const auto &plugin : plugins)
            {
                if (plugin->on_udp_packet(pluginId, payload, payload_size))
                {
                    // Packet was handled by the plugin
                    break;
                }
            }

            offset += 7 + payload_size;
        }

        // Remove processed data from buffer
        if (offset > 0)
        {
            packet_buffer.erase(packet_buffer.begin(), packet_buffer.begin() + offset);
        }

        // Prevent buffer from growing too large
        if (packet_buffer.size() > buffer_size)
        {
            spdlog::warn("Packet buffer too large, clearing incomplete packets");
            packet_buffer.clear();
        }
    }
}

UdpServer::UdpServer(int port) : server_running(true)
{
    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        spdlog::error("Failed to create UDP socket: {}", strerror(errno));
        return;
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Set socket options for reuse
    int reuse = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        spdlog::error("Failed to set socket options: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(udp_socket, F_GETFL, 0);
    if (flags < 0)
    {
        spdlog::error("Failed to get socket flags: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }
    if (fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        spdlog::error("Failed to set socket to non-blocking: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }

    // Bind socket
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        spdlog::error("Failed to bind UDP socket: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }

    // Start server thread
    server_running = true;
    udp_server_thread = std::thread(&UdpServer::server_loop, this);
    spdlog::info("UDP server started on port {}", port);
}

UdpServer::~UdpServer()
{

    server_running = false;

    spdlog::info("Stopping UDP server...");
    if (udp_server_thread.joinable())
    {
        udp_server_thread.join();
    }

    if (udp_socket >= 0)
    {
        close(udp_socket);
        udp_socket = -1;
    }

    spdlog::info("UDP server stopped");
}

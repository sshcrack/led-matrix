#include "udp.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <spdlog/spdlog.h>

void UdpServer::server_loop()
{
    constexpr size_t buffer_size = 1024;
    char buffer[buffer_size];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    auto plugins = PluginManager::instance()->get_plugins();
    while (server_running)
    {
        ssize_t n = recvfrom(udp_socket, buffer, buffer_size, 0,
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

        // Process received packet
        // Check packet format based on the Rust code:
        // - Magic number (2 bytes): 0xAD, 0x01
        // - Version (1 byte): (Something plugin-specific)

        if (n < 3)
        {
            // Packet too small
            continue;
        }

        const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);
        const uint8_t magicPacket = data[1] != 0x01;
        const uint8_t version = data[2];

        // Check magic number and version
        if (data[0] != 0xAD)
        {
            // Invalid packet
            continue;
        }

        for (auto &plugin : plugins)
        {
            if (plugin->on_udp_packet(magicPacket, version, data + 3, n - 3))
            {
                // Packet was handled by the plugin
                break;
            }
        }
    }
}

UdpServer::UdpServer(int port)
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
    udp_server_thread = std::thread(&UdpServer::udp_server_loop, this);
    spdlog::info("UDP server started on port {}", port);
}

UdpServer::~UdpServer()
{

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

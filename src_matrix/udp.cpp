#include "udp.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/diagnostics.h>

void UdpServer::server_loop()
{
    constexpr size_t buffer_size = 64 * 1024;
    constexpr size_t packet_header_size = 7;
    std::vector<uint8_t> receive_buffer(buffer_size);
    struct sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);

    spdlog::info("Getting plugins...");
    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    spdlog::info("Done. Found {} plugins.", plugins.size());

    while (server_running)
    {
        const ssize_t n = recvfrom(udp_socket, receive_buffer.data(), receive_buffer.size(), 0,
                                   reinterpret_cast<struct sockaddr *>(&client_addr), &client_addr_len);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (errno == EINTR)
                continue;
            spdlog::error("UDP recvfrom error: {}", strerror(errno));
            break;
        }

        const auto datagram_size = static_cast<size_t>(n);
        Diagnostics::RuntimeDiagnostics::instance().record_udp_datagram(datagram_size);

        // UDP preserves datagram boundaries and IP fragmentation is reassembled
        // by the kernel. Never carry an incomplete application packet into the
        // next datagram: doing so creates artificial latency and can concatenate
        // unrelated packets after a truncated/corrupt datagram.
        size_t offset = 0;
        while (offset < datagram_size)
        {
            if (datagram_size - offset < packet_header_size)
            {
                Diagnostics::RuntimeDiagnostics::instance().record_udp_malformed();
                break;
            }

            const uint8_t *data = receive_buffer.data() + offset;
            if (data[0] != 0xAD || data[1] != 0x01)
            {
                Diagnostics::RuntimeDiagnostics::instance().record_udp_malformed();
                break;
            }

            const uint8_t plugin_id = data[2];
            const uint32_t payload_size = (static_cast<uint32_t>(data[3]) << 24) |
                                          (static_cast<uint32_t>(data[4]) << 16) |
                                          (static_cast<uint32_t>(data[5]) << 8) |
                                          static_cast<uint32_t>(data[6]);
            const size_t framed_size = packet_header_size + static_cast<size_t>(payload_size);
            if (framed_size > datagram_size - offset)
            {
                Diagnostics::RuntimeDiagnostics::instance().record_udp_malformed();
                spdlog::debug("Dropping truncated UDP packet: declared {} payload bytes, datagram has {} bytes left",
                              payload_size, datagram_size - offset - packet_header_size);
                break;
            }

            const uint8_t *payload = data + packet_header_size;
            bool handled = false;
            for (const auto &plugin : plugins)
            {
                try {
                    if (plugin->on_udp_packet(plugin_id, payload, payload_size))
                    {
                        handled = true;
                        break;
                    }
                } catch (const std::exception &e) {
                    spdlog::error("Plugin '{}' threw while handling UDP packet {}: {}",
                                  plugin->get_plugin_name(), plugin_id, e.what());
                } catch (...) {
                    spdlog::error("Plugin '{}' threw an unknown exception while handling UDP packet {}",
                                  plugin->get_plugin_name(), plugin_id);
                }
            }
            Diagnostics::RuntimeDiagnostics::instance().record_udp_packet(handled);
            offset += framed_size;
        }
    }
}

UdpServer::UdpServer(int port) : udp_socket(-1), server_running(false)
{
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        spdlog::error("Failed to create UDP socket: {}", strerror(errno));
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int reuse = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        spdlog::error("Failed to set socket options: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }

    constexpr int rcvbuf_size = 256 * 1024;
    int rcvbuf = rcvbuf_size;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0)
    {
        spdlog::warn("Failed to set SO_RCVBUF: {}", strerror(errno));
    }

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

    if (bind(udp_socket, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        spdlog::error("Failed to bind UDP socket: {}", strerror(errno));
        close(udp_socket);
        udp_socket = -1;
        return;
    }

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

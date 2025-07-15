#include "AudioVisualizer.h"
#include "scenes/AudioSpectrumScene.h"
#include "spdlog/spdlog.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <thread>
#include <chrono>

using namespace Scenes;

extern "C" [[maybe_unused]] [[maybe_unused]] AudioVisualizer *createAudioVisualizer()
{
    return new AudioVisualizer();
}

extern "C" [[maybe_unused]] void destroyAudioVisualizer(AudioVisualizer *c)
{
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> AudioVisualizer::create_image_providers()
{
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> AudioVisualizer::create_scenes()
{
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    auto deleteScene = [](SceneWrapper *scene)
    {
        delete scene;
    };

    scenes.push_back({new AudioSpectrumSceneWrapper(), deleteScene});

    return scenes;
}

AudioVisualizer::AudioVisualizer() : server_running(false), udp_socket(-1), last_timestamp(0), interpolated_log(false)
{
    spdlog::info("AudioVisualizer plugin initialized");

    current_audio_data.resize(64);
    std::fill(current_audio_data.begin(), current_audio_data.end(), 0);
}

AudioVisualizer::~AudioVisualizer()
{
    stop_udp_server();
}

std::optional<string> AudioVisualizer::before_server_init()
{
    spdlog::info("Starting UDP server for audio visualization");
    start_udp_server(8888); // Use port 8888 for UDP audio data
    return std::nullopt;
}

std::optional<string> AudioVisualizer::pre_exit()
{
    spdlog::info("Stopping UDP server for audio visualization");
    stop_udp_server();
    return std::nullopt;
}

void AudioVisualizer::start_udp_server(int port)
{
    if (server_running)
    {
        spdlog::warn("UDP server already running");
        return;
    }

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
    udp_server_thread = std::thread(&AudioVisualizer::udp_server_loop, this);
    spdlog::info("UDP server started on port {}", port);
}

void AudioVisualizer::stop_udp_server()
{
    if (!server_running)
    {
        return;
    }

    server_running = false;

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

void AudioVisualizer::udp_server_loop()
{
    constexpr size_t buffer_size = 1024;
    char buffer[buffer_size];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

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
        // - Version (1 byte): 0x01
        // - Number of bands (1 byte)
        // - Flags (1 byte)
        // - Timestamp (4 bytes)
        // - Bands data (num_bands bytes)

        if (n < 9)
        {
            // Packet too small
            continue;
        }

        const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer);

        // Check magic number and version
        if (data[0] != 0xAD || data[1] != 0x01 || data[2] != 0x01)
        {
            // Invalid packet
            continue;
        }

        uint8_t num_bands = data[3];
        uint8_t flags = data[4];

        // Extract timestamp (little endian)
        uint32_t timestamp =
            static_cast<uint32_t>(data[5]) |
            (static_cast<uint32_t>(data[6]) << 8) |
            (static_cast<uint32_t>(data[7]) << 16) |
            (static_cast<uint32_t>(data[8]) << 24);

        // Check if packet size matches expected size
        if (n != 9 + num_bands)
        {
            // Invalid packet size
            continue;
        }

        // Extract bit 0 of flags which indicates if interpolated_log is enabled
        bool is_interpolated_log = (flags & 0x01) != 0;

        // Update audio data
        {
            std::lock_guard<std::mutex> lock(audio_data_mutex);
            current_audio_data.assign(data + 9, data + 9 + num_bands);
            last_timestamp = timestamp;
            interpolated_log = is_interpolated_log;
        }
    }
}

std::vector<uint8_t> AudioVisualizer::get_audio_data()
{
    std::lock_guard<std::mutex> lock(audio_data_mutex);
    return current_audio_data;
}

bool AudioVisualizer::get_interpolated_log_state()
{
    std::lock_guard<std::mutex> lock(audio_data_mutex);
    return interpolated_log;
}

uint32_t AudioVisualizer::get_last_timestamp()
{
    std::lock_guard<std::mutex> lock(audio_data_mutex);
    return last_timestamp;
}

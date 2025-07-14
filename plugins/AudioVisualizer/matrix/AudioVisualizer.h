#pragma once

#include "plugin/main.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <netinet/in.h>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class AudioVisualizer : public BasicPlugin {
private:
    std::thread udp_server_thread;
    std::atomic<bool> server_running;
    std::mutex audio_data_mutex;
    std::vector<uint8_t> current_audio_data;
    int udp_socket;
    struct sockaddr_in server_addr;
    uint32_t last_timestamp;
    bool interpolated_log;

public:
    AudioVisualizer();
    ~AudioVisualizer() override;

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> create_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> create_image_providers() override;
    
    std::optional<string> before_server_init() override;
    std::optional<string> pre_exit() override;

    // Methods for the UDP server
    void start_udp_server(int port);
    void stop_udp_server();
    void udp_server_loop();
    
    // Method to get audio data for scenes
    std::vector<uint8_t> get_audio_data();
    bool get_interpolated_log_state();
    uint32_t get_last_timestamp();
};

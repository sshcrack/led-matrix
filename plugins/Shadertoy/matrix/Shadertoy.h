#pragma once

#include "shared/matrix/plugin/main.h"
#include <mutex>
#include <vector>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class AudioVisualizer : public BasicPlugin {
    std::mutex audio_data_mutex;
    std::vector<uint8_t> current_audio_data;
    uint32_t last_timestamp;
    bool interpolated_log;

public:
    AudioVisualizer();

    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> > create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_image_providers() override;

    std::optional<string> before_server_init() override;

    std::optional<string> pre_exit() override;

    // Method to get audio data for scenes
    std::vector<uint8_t> get_audio_data();

    bool get_interpolated_log_state();

    uint32_t get_last_timestamp();

    bool on_udp_packet(const uint8_t magicPacket, const uint8_t version, const uint8_t *data,
                       const size_t size) override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }
};

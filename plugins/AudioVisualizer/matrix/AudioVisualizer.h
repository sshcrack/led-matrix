#pragma once

#include "shared/matrix/plugin/main.h"
#include <mutex>
#include <vector>
#include <deque>
#include <chrono>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

struct BeatDetectionParams {
    float energy_threshold_multiplier = 1.5f;  // Multiplier for average energy to trigger beat
    int energy_history_size = 43;              // Number of frames to track for average energy
    float min_beat_interval = 0.3f;            // Minimum time between beats in seconds
    float decay_factor = 0.95f;                // How quickly energy decays
};

class AudioVisualizer : public BasicPlugin {
    std::mutex audio_data_mutex;
    std::vector<uint8_t> current_audio_data;
    uint32_t last_timestamp;
    bool interpolated_log;
    
    // Beat detection members
    std::deque<float> energy_history;
    std::chrono::steady_clock::time_point last_beat_time;
    BeatDetectionParams beat_params;
    bool beat_detected;

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

    // Beat detection methods (override from BasicPlugin)
    bool is_beat_detected() override;
    void clear_beat_flag() override;

    bool on_udp_packet(const uint8_t pluginId, const uint8_t *data,
                       const size_t size) override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }

private:
    // Internal beat detection method
    bool detect_beat(const std::vector<uint8_t>& audio_data);
    float calculate_energy(const std::vector<uint8_t>& audio_data);
    void send_beat_message();
};

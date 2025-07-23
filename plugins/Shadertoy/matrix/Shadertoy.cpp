#include "AudioVisualizer.h"
#include "scenes/AudioSpectrumScene.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <thread>
#include <chrono>

using namespace Scenes;

extern "C" PLUGIN_EXPORT AudioVisualizer *createAudioVisualizer()
{
    return new AudioVisualizer();
}

extern "C" PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizer *c)
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

AudioVisualizer::AudioVisualizer() : last_timestamp(0), interpolated_log(false)
{
    spdlog::info("AudioVisualizer plugin initialized");

    current_audio_data.resize(64);
    std::fill(current_audio_data.begin(), current_audio_data.end(), 0);
}

std::optional<string> AudioVisualizer::before_server_init()
{
    spdlog::info("Starting UDP server for audio visualization");
    return std::nullopt;
}

std::optional<string> AudioVisualizer::pre_exit()
{
    spdlog::info("Stopping UDP server for audio visualization");
    return std::nullopt;
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

bool AudioVisualizer::on_udp_packet(const uint8_t magicPacket, const uint8_t version, const uint8_t *data, const size_t size)
{
    // Process received packet
    // Check packet format based on the Rust code (previous implementation):
    // - Number of bands (1 byte)
    // - Flags (1 byte)
    // - Timestamp (4 bytes)
    // - Bands data (num_bands bytes)
    if(magicPacket != 0x01)
        return false; // Not destined for this plugin

    if (version != 0x01)
        return false; // Invalid version

    if (size < 6)
        return false;

    uint8_t num_bands = data[0];
    uint8_t flags = data[1];

    // Extract timestamp (little endian)
    uint32_t timestamp =
        static_cast<uint32_t>(data[2]) |
        (static_cast<uint32_t>(data[3]) << 8) |
        (static_cast<uint32_t>(data[4]) << 16) |
        (static_cast<uint32_t>(data[5]) << 24);

    // Check if packet size matches expected size
    if (size != 6 + num_bands)
    {
        // Invalid packet size
        return false;
    }

    // Extract bit 0 of flags which indicates if interpolated_log is enabled
    bool is_interpolated_log = (flags & 0x01) != 0;

    // Update audio data
    {
        std::lock_guard<std::mutex> lock(audio_data_mutex);
        current_audio_data.assign(data + 6, data + 6 + num_bands);
        last_timestamp = timestamp;
        interpolated_log = is_interpolated_log;
    }

    return true;
}
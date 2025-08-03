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

AudioVisualizer::AudioVisualizer() : last_timestamp(0), interpolated_log(false), beat_detected(false)
{
    current_audio_data.resize(64);
    std::ranges::fill(current_audio_data, 0);
    last_beat_time = std::chrono::steady_clock::now();
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

bool AudioVisualizer::on_udp_packet(const uint8_t pluginId, const uint8_t *data, const size_t size)
{
    // Process received packet
    // Check packet format based on the Rust code (previous implementation):
    // - Number of bands (1 byte)
    // - Flags (1 byte)
    // - Timestamp (4 bytes)
    // - Bands data (num_bands bytes)
    if(pluginId != 0x01)
        return false; // Not destined for this plugin

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
        
        // Perform beat detection on new audio data
        if (detect_beat(current_audio_data)) {
            send_beat_message();
        }
    }

    return true;
}

bool AudioVisualizer::is_beat_detected()
{
    std::lock_guard<std::mutex> lock(audio_data_mutex);
    return beat_detected;
}

void AudioVisualizer::clear_beat_flag()
{
    std::lock_guard<std::mutex> lock(audio_data_mutex);
    beat_detected = false;
}

bool AudioVisualizer::detect_beat(const std::vector<uint8_t>& audio_data)
{
    if (audio_data.empty()) {
        return false;
    }
    
    // Calculate current energy
    float current_energy = calculate_energy(audio_data);
    
    // Add to energy history
    energy_history.push_back(current_energy);
    if (energy_history.size() > beat_params.energy_history_size) {
        energy_history.pop_front();
    }
    
    // Need sufficient history to detect beats
    if (energy_history.size() < beat_params.energy_history_size / 2) {
        return false;
    }
    
    // Calculate average energy over history
    float avg_energy = 0.0f;
    for (float energy : energy_history) {
        avg_energy += energy;
    }
    avg_energy /= energy_history.size();
    
    // Check for beat: current energy significantly higher than average
    bool is_beat = current_energy > (avg_energy * beat_params.energy_threshold_multiplier);
    
    // Apply minimum time constraint between beats
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_beat = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_beat_time).count();
    
    if (is_beat && time_since_last_beat >= beat_params.min_beat_interval) {
        last_beat_time = now;
        beat_detected = true;
        spdlog::debug("Beat detected! Energy: {:.2f}, Avg: {:.2f}, Ratio: {:.2f}", 
                     current_energy, avg_energy, current_energy / avg_energy);
        return true;
    }
    
    return false;
}

float AudioVisualizer::calculate_energy(const std::vector<uint8_t>& audio_data)
{
    float total_energy = 0.0f;
    
    // Focus on lower frequency bands for beat detection (typically where bass is)
    int focus_bands = std::min(static_cast<int>(audio_data.size()), 16);
    
    for (int i = 0; i < focus_bands; i++) {
        float normalized = audio_data[i] / 255.0f;
        total_energy += normalized * normalized; // Square for energy
    }
    
    return total_energy / focus_bands;
}

void AudioVisualizer::send_beat_message()
{
    // Create a beat detection message to be sent to the matrix controller
    // We'll use the existing UDP infrastructure to send a special beat message
    // Format: plugin_id = 0x02 (beat detection), payload = beat timestamp
    
    // For now, we'll just log the beat detection
    // The matrix controller post-processing will check the beat flag via is_beat_detected()
    spdlog::info("Beat detected - triggering post-processing effects");
}
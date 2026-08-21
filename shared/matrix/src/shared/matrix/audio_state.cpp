#include <shared/matrix/audio_state.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>

#include <algorithm>
#include <chrono>
#include <mutex>

namespace {
std::mutex stateMutex;
AudioProtocol::Frame currentFrame;
std::chrono::steady_clock::time_point receivedAt{};
bool hasFrame = false;
}

namespace AudioState {
void update(const AudioProtocol::Frame &frame) {
    {
        std::lock_guard lock(stateMutex);
        currentFrame = frame;
        receivedAt = std::chrono::steady_clock::now();
        hasFrame = true;
    }

    RuntimeInputs::publish(
        RuntimeInputIds::Audio,
        {
            {"rms", static_cast<double>(frame.feature(AudioProtocol::Feature::Rms))},
            {"loudness", static_cast<double>(frame.feature(AudioProtocol::Feature::Loudness))},
            {"sub_bass", static_cast<double>(frame.feature(AudioProtocol::Feature::SubBass))},
            {"bass", static_cast<double>(frame.feature(AudioProtocol::Feature::Bass))},
            {"mid", static_cast<double>(frame.feature(AudioProtocol::Feature::Mid))},
            {"treble", static_cast<double>(frame.feature(AudioProtocol::Feature::Treble))},
            {"energy_trend", static_cast<double>(frame.feature(AudioProtocol::Feature::EnergyTrend))},
            {"bpm", static_cast<double>(frame.feature(AudioProtocol::Feature::Bpm))},
            {"beat_phase", static_cast<double>(frame.feature(AudioProtocol::Feature::BeatPhase))},
            {"beat_confidence", static_cast<double>(frame.feature(AudioProtocol::Feature::BeatConfidence))},
            {"beat_strength", static_cast<double>(frame.feature(AudioProtocol::Feature::BeatStrength))},
            {"stereo_width", static_cast<double>(frame.feature(AudioProtocol::Feature::StereoWidth))},
            {"stereo_balance", static_cast<double>(frame.feature(AudioProtocol::Feature::StereoBalance))},
            {"silence", frame.event(AudioProtocol::Silent)},
            {"beat_counter", static_cast<std::int64_t>(frame.beat_counter)},
            {"drop_counter", static_cast<std::int64_t>(frame.drop_counter)},
            {"section_counter", static_cast<std::int64_t>(frame.section_counter)}
        },
        std::chrono::milliseconds(750));
}

void clear() {
    {
        std::lock_guard lock(stateMutex);
        currentFrame = {};
        receivedAt = {};
        hasFrame = false;
    }
    RuntimeInputs::set_available(RuntimeInputIds::Audio, false);
}

Snapshot snapshot() {
    std::lock_guard lock(stateMutex);
    Snapshot result;
    static_cast<AudioProtocol::Frame &>(result) = currentFrame;
    result.available = hasFrame;
    if (hasFrame)
        result.age_seconds = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - receivedAt).count();
    return result;
}

float average_spectrum(const Snapshot &state, float start_fraction, float end_fraction) {
    if (state.spectrum.empty()) return 0.0f;
    start_fraction = std::clamp(start_fraction, 0.0f, 1.0f);
    end_fraction = std::clamp(end_fraction, start_fraction, 1.0f);
    const size_t begin = std::min(state.spectrum.size() - 1,
        static_cast<size_t>(start_fraction * static_cast<float>(state.spectrum.size())));
    const size_t end = std::max(begin + 1, std::min(state.spectrum.size(),
        static_cast<size_t>(end_fraction * static_cast<float>(state.spectrum.size()))));
    float sum = 0.0f;
    for (size_t i = begin; i < end; ++i) sum += state.spectrum[i];
    return sum / static_cast<float>(end - begin);
}
}

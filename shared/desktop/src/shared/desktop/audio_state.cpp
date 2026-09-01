#include <shared/desktop/audio_state.h>

#include <chrono>
#include <mutex>

namespace {
std::mutex stateMutex;
AudioProtocol::Frame currentFrame;
std::chrono::steady_clock::time_point receivedAt{};
float currentSampleRate = 0.0f;
bool hasFrame = false;
}

namespace DesktopAudioState {

void update(const AudioProtocol::Frame &frame, float sample_rate) {
    std::lock_guard lock(stateMutex);
    currentFrame = frame;
    currentSampleRate = sample_rate;
    receivedAt = std::chrono::steady_clock::now();
    hasFrame = true;
}

void clear() {
    std::lock_guard lock(stateMutex);
    currentFrame = {};
    currentSampleRate = 0.0f;
    receivedAt = {};
    hasFrame = false;
}

Snapshot snapshot() {
    std::lock_guard lock(stateMutex);
    Snapshot result;
    static_cast<AudioProtocol::Frame &>(result) = currentFrame;
    result.available = hasFrame;
    result.sample_rate = currentSampleRate;
    if (hasFrame) {
        result.age_seconds = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - receivedAt).count();
    }
    return result;
}

} // namespace DesktopAudioState

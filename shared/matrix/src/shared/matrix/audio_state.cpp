#include <shared/matrix/audio_state.h>

#include <algorithm>
#include <mutex>

namespace {
std::mutex stateMutex;
AudioProtocol::Frame currentFrame;
std::chrono::steady_clock::time_point receivedAt{};
bool hasFrame = false;
}

namespace AudioState {
void update(const AudioProtocol::Frame &frame) {
    std::lock_guard lock(stateMutex);
    currentFrame = frame;
    receivedAt = std::chrono::steady_clock::now();
    hasFrame = true;
}

void clear() {
    std::lock_guard lock(stateMutex);
    currentFrame = {};
    receivedAt = {};
    hasFrame = false;
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

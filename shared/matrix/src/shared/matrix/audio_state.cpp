#include <shared/matrix/audio_state.h>
#include <algorithm>
#include <mutex>

namespace {
std::mutex state_mutex;
AudioState::Snapshot current_state;
}

namespace AudioState {
void update(const std::vector<uint8_t>& bands, uint32_t timestamp, uint64_t beat_counter) {
    std::lock_guard lock(state_mutex);
    current_state.bands = bands;
    current_state.timestamp = timestamp;
    current_state.beat_counter = beat_counter;
    current_state.available = !bands.empty();
}

Snapshot snapshot() {
    std::lock_guard lock(state_mutex);
    return current_state;
}

float average_band(const Snapshot& state, float start_fraction, float end_fraction) {
    if (state.bands.empty()) return 0.0f;
    start_fraction = std::clamp(start_fraction, 0.0f, 1.0f);
    end_fraction = std::clamp(end_fraction, start_fraction, 1.0f);
    const size_t begin = std::min(state.bands.size() - 1,
        static_cast<size_t>(start_fraction * static_cast<float>(state.bands.size())));
    const size_t end = std::max(begin + 1, std::min(state.bands.size(),
        static_cast<size_t>(end_fraction * static_cast<float>(state.bands.size()))));
    float sum = 0.0f;
    for (size_t i = begin; i < end; ++i) sum += state.bands[i] / 255.0f;
    return sum / static_cast<float>(end - begin);
}
}

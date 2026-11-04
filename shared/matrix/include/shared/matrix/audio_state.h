#pragma once

#include <shared/common/audio_protocol.h>
#include <chrono>

namespace AudioState {

struct Snapshot : AudioProtocol::Frame {
    bool available = false;
    float age_seconds = 1000.0f;

    [[nodiscard]] bool fresh(float max_age_seconds = 0.5f) const {
        return available && age_seconds <= max_age_seconds;
    }
};

void update(const AudioProtocol::Frame &frame);
/// Same as update(), with an explicit receive time. Offline replays (preview
/// fixtures, director simulations) use it to run faster than real time while
/// keeping the style estimator and Runtime Input cadence on audio time.
void update(const AudioProtocol::Frame &frame, std::chrono::steady_clock::time_point now);
void clear();
Snapshot snapshot();
float average_spectrum(const Snapshot &state, float start_fraction, float end_fraction);

} // namespace AudioState

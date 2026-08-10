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
void clear();
Snapshot snapshot();
float average_spectrum(const Snapshot &state, float start_fraction, float end_fraction);

} // namespace AudioState

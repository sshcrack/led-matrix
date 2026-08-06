#pragma once

#include <cstdint>
#include <vector>

namespace AudioState {
struct Snapshot {
    std::vector<uint8_t> bands;
    uint32_t timestamp = 0;
    uint64_t beat_counter = 0;
    bool available = false;
};

void update(const std::vector<uint8_t>& bands, uint32_t timestamp, uint64_t beat_counter);
Snapshot snapshot();
float average_band(const Snapshot& state, float start_fraction, float end_fraction);
}

#pragma once

#include "./track.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/utils.h"

#include <chrono>

struct SpotifyState {
private:
    const nlohmann::json state_json;
    SpotifyTrack track;

public:
    explicit SpotifyState(nlohmann::json state_json);

    SpotifyTrack get_track();

    long get_progress_ms();

    std::optional<float> get_progress(tmillis_t additional_ms = 0);

    bool is_playing();
};

std::optional<SpotifyState> spotify_state_from_runtime_input(
    const RuntimeInputs::Snapshot& snapshot,
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

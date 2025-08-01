#pragma once

#include "./track.h"
#include "nlohmann/json.hpp"
#include "shared/matrix/utils/utils.h"

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
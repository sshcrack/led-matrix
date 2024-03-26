#pragma once

#include "./track.h"
#include <nlohmann/json.hpp>

struct SpotifyState {
private:
    const nlohmann::json state_json;
    SpotifyTrack track;


public:
    explicit SpotifyState(nlohmann::json state_json);

    SpotifyTrack get_track();

    long get_progress_ms();

    float get_progress();
};
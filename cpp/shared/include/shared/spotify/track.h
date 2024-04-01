#pragma once
#include <nlohmann/json.hpp>

struct SpotifyTrack {
private:
const nlohmann::json track_json;

public:
    explicit SpotifyTrack(nlohmann::json track_json);
    long get_duration();
    std::string get_id();
    std::optional<std::string> get_cover();
};
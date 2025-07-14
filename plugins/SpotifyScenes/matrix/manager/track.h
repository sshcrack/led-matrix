#pragma once
#include "nlohmann/json.hpp"

struct SpotifyTrack {
private:
const nlohmann::json track_json;

public:
    explicit SpotifyTrack(nlohmann::json track_json);
    std::optional<long> get_duration() const;
    std::optional<std::string> get_id() const;
    std::optional<std::string> get_cover() const;
    std::optional<std::string> get_song_name() const;
    std::optional<std::string> get_artist_name() const;
};
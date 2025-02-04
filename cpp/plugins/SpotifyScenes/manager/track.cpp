
#include "track.h"

#include <utility>

SpotifyTrack::SpotifyTrack(nlohmann::json t_json) : track_json(std::move(t_json)) {}

long SpotifyTrack::get_duration() {
    return this->track_json["duration_ms"].template get<long>();
}

std::optional<std::string> SpotifyTrack::get_cover() {
    try {
        return this->track_json["album"]["images"][0]["url"].template get<std::string>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::string SpotifyTrack::get_id() {
    return this->track_json["id"].template get<std::string>();
}

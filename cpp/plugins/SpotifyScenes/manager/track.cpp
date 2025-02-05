
#include "track.h"

#include <utility>

SpotifyTrack::SpotifyTrack(nlohmann::json t_json) : track_json(std::move(t_json)) {}

std::optional<long> SpotifyTrack::get_duration() {
    try {
        return this->track_json["duration_ms"].template get<long>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_cover() {
    try {
        return this->track_json["album"]["images"][0]["url"].template get<std::string>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_id() {
    try {
        return this->track_json["id"].template get<std::string>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

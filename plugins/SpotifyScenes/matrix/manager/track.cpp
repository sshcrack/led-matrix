
#include "track.h"

#include <utility>

SpotifyTrack::SpotifyTrack(nlohmann::json t_json) : track_json(std::move(t_json)) {}

std::optional<long> SpotifyTrack::get_duration() const {
    try {
        return this->track_json["duration_ms"].template get<long>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_cover() const {
    try {
        return this->track_json["album"]["images"][0]["url"].template get<std::string>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_song_name() const {
    try {
        return this->track_json["name"];
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_artist_name() const {
    try {
        return this->track_json["artists"][0]["name"];
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_id() const {
    try {
        return this->track_json["id"].template get<std::string>();
    } catch (std::exception& ex) {
        return std::nullopt;
    }
}

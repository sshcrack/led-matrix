
#include "track.h"

#include <utility>

SpotifyTrack::SpotifyTrack(nlohmann::json t_json) : track_json(std::move(t_json)) {}

std::optional<long> SpotifyTrack::get_duration() const {
    try {
        return this->track_json["duration_ms"].template get<long>();
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_cover() const {
    try {
        return this->track_json["album"]["images"][0]["url"].template get<std::string>();
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_song_name() const {
    try {
        return this->track_json["name"].template get<std::string>();
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string> SpotifyTrack::get_artist_name() const {
    try {
        return this->track_json["artists"][0]["name"].template get<std::string>();
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::string> SpotifyTrack::get_artist_ids() const {
    std::vector<std::string> ids;
    if (!track_json.contains("artists") || !track_json["artists"].is_array())
        return ids;
    for (const auto &artist : track_json["artists"])
        if (artist.contains("id") && artist["id"].is_string())
            ids.push_back(artist["id"].get<std::string>());
    return ids;
}

std::optional<std::string> SpotifyTrack::get_id() const {
    try {
        return this->track_json["id"].template get<std::string>();
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

#pragma once

#include <string_view>

namespace RuntimeInputIds {
inline constexpr std::string_view Desktop = "desktop";
inline constexpr std::string_view Audio = "audio";
inline constexpr std::string_view SpotifyPlayback = "spotify.playback";
inline constexpr std::string_view SpotifyMVReady = "spotify.mv.ready";
// Genre metadata for the currently playing track (SpotifyScenes genre lookup):
// {track_id, genres: "comma,separated", source}.
inline constexpr std::string_view MusicGenre = "music.genre";
// Real-world environment + user overrides (Environment::Service):
// {real_world, latitude?, longitude?, weather_code?, ..., time_override,
//  weather_override, season_override, occasions}. Read via Environment::current().
inline constexpr std::string_view Environment = "environment";
} // namespace RuntimeInputIds

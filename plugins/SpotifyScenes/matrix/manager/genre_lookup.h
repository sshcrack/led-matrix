#pragma once

// Genre metadata for the current Spotify track.
//
// Sources, in order:
//   * Spotify artist genres (fine-grained, e.g. "german hip hop", "pop punk")
//   * Deezer (no key) track search -> album genres, when Spotify has none
//     (Spotify may stop returning genres for development-mode apps)
//   * GetSongBPM (optional, pluginConfigs.song_bpm_api_key): artist genres
// Lookups run on a background thread, are cached by Spotify track id (memory
// + cache/genres.json) and are published as RuntimeInputIds::MusicGenre so the
// directors can match scenes to the music style.

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class GenreLookup {
public:
    struct Result {
        std::string genres;  // comma separated, may be empty (looked up, nothing found)
        std::string source;  // "deezer", "getsongbpm", "deezer+getsongbpm"
    };

    /// Authenticated Spotify GET (full URL); empty on any failure.
    using SpotifyFetch = std::function<std::optional<nlohmann::json>(const std::string &url)>;

    static GenreLookup &instance();
    void set_spotify_fetch(SpotifyFetch fetch);

    /// Non-blocking: publish the cached genres for this track or queue a lookup.
    void track_playing(const std::string &track_id, const std::string &title, const std::string &artist,
                       std::vector<std::string> artist_ids = {});
    void stop();

    ~GenreLookup();

private:
    GenreLookup() = default;
    void worker(std::stop_token stop);
    void publish(const std::string &track_id, const Result &result) const;
    void load_cache();
    void save_cache() const;
    [[nodiscard]] static std::filesystem::path cache_path();

    std::mutex mtx_;
    std::condition_variable_any cv_;
    std::jthread thread_;
    bool loaded_ = false;
    std::unordered_map<std::string, Result> cache_;
    struct Pending {
        std::string track_id, title, artist;
        std::vector<std::string> artist_ids;
    };
    SpotifyFetch spotify_fetch_;
    std::vector<std::string> spotify_genres(const std::vector<std::string> &artist_ids);
    std::optional<Pending> pending_;
    std::string in_flight_;
};

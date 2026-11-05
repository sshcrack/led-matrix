#include "genre_lookup.h"

#include <chrono>
#include <fstream>
#include <set>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shared/matrix/input_ids.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/shared.h"

namespace {
using nlohmann::json;
constexpr auto UserAgent = "led-matrix (https://github.com/sshcrack/led-matrix)";

// Set when a request failed (network, rate limit, server error) rather than
// answering "nothing found"; failed lookups are retried later, not cached.
thread_local bool request_failed = false;

std::optional<json> get_json(const std::string &url, const std::stop_token &stop)
{
    const auto response = cpr::Get(cpr::Url{url}, cpr::Header{{"User-Agent", UserAgent}, {"Accept-Language", "en"}},
                                   cpr::ConnectTimeout{std::chrono::seconds(5)}, cpr::Timeout{std::chrono::seconds(15)},
                                   cpr::ProgressCallback{[stop](auto, auto, auto, auto, intptr_t) { return !stop.stop_requested(); }});
    if (response.status_code != 200) {
        if (response.status_code != 404)
            request_failed = true;
        return std::nullopt;
    }
    try {
        return json::parse(response.text);
    } catch (...) {
        return std::nullopt;
    }
}

std::string lower(std::string s)
{
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Deezer: best track match for "artist title", then its album's genres.
std::vector<std::string> deezer_genres(const std::string &title, const std::string &artist, const std::stop_token &stop)
{
    const auto q = cpr::util::urlEncode(artist + " " + title);
    const auto search = get_json("https://api.deezer.com/search?limit=5&q=" + std::string(q), stop);
    if (!search || !search->contains("data") || !(*search)["data"].is_array())
        return {};
    const auto wantedArtist = lower(artist);
    std::optional<std::int64_t> album;
    for (const auto &hit : (*search)["data"]) {
        const auto hitArtist = lower(hit.value("/artist/name"_json_pointer, std::string{}));
        if (!hit.contains("album") || !hit["album"].contains("id"))
            continue;
        // Prefer a hit by the same artist; fall back to the top hit.
        if (!album || (!wantedArtist.empty() && (hitArtist.find(wantedArtist) != std::string::npos ||
                                                 wantedArtist.find(hitArtist) != std::string::npos))) {
            album = hit["album"]["id"].get<std::int64_t>();
            if (hitArtist == wantedArtist)
                break;
        }
    }
    if (!album)
        return {};
    const auto info = get_json("https://api.deezer.com/album/" + std::to_string(*album), stop);
    std::vector<std::string> genres;
    if (info && info->contains("genres") && (*info)["genres"].contains("data"))
        for (const auto &g : (*info)["genres"]["data"])
            if (g.contains("name") && g["name"].is_string())
                genres.push_back(g["name"].get<std::string>());
    return genres;
}

/// GetSongBPM: artist genres (mainstream artists only).
std::vector<std::string> getsongbpm_genres(const std::string &title, const std::string &artist, const std::stop_token &stop)
{
    const auto configs = config->get_plugin_configs();
    const auto key = configs.find("song_bpm_api_key");
    if (key == configs.end() || key->second.empty())
        return {};
    const auto url = "https://api.getsong.co/search/?type=both&limit=1&lookup=song:" + std::string(cpr::util::urlEncode(title)) +
                     "%20artist:" + std::string(cpr::util::urlEncode(artist)) + "&api_key=" + key->second;
    const auto j = get_json(url, stop);
    std::vector<std::string> genres;
    if (j && j->contains("search") && (*j)["search"].is_array() && !(*j)["search"].empty()) {
        const auto &hit = (*j)["search"][0];
        if (hit.contains("artist") && hit["artist"].contains("genres") && hit["artist"]["genres"].is_array())
            for (const auto &g : hit["artist"]["genres"])
                if (g.is_string())
                    genres.push_back(g.get<std::string>());
    }
    return genres;
}
}  // namespace

GenreLookup &GenreLookup::instance()
{
    static GenreLookup lookup;
    return lookup;
}

GenreLookup::~GenreLookup() { stop(); }

void GenreLookup::set_spotify_fetch(SpotifyFetch fetch)
{
    std::lock_guard lock(mtx_);
    spotify_fetch_ = std::move(fetch);
}

std::vector<std::string> GenreLookup::spotify_genres(const std::vector<std::string> &artist_ids)
{
    SpotifyFetch fetch;
    {
        std::lock_guard lock(mtx_);
        fetch = spotify_fetch_;
    }
    std::vector<std::string> genres;
    if (!fetch)
        return genres;
    // The main artist decides; features add a little (max 3 requests).
    for (std::size_t i = 0; i < std::min<std::size_t>(3, artist_ids.size()); ++i) {
        const auto artist = fetch("https://api.spotify.com/v1/artists/" + artist_ids[i]);
        if (!artist)
            request_failed = true;
        if (artist && artist->contains("genres") && (*artist)["genres"].is_array())
            for (const auto &g : (*artist)["genres"])
                if (g.is_string())
                    genres.push_back(g.get<std::string>());
    }
    return genres;
}

std::filesystem::path GenreLookup::cache_path() { return std::filesystem::current_path() / "cache" / "genres.json"; }

void GenreLookup::load_cache()
{
    loaded_ = true;
    std::ifstream in(cache_path());
    if (!in)
        return;
    try {
        const auto j = json::parse(in);
        for (const auto &[id, entry] : j.items())
            cache_[id] = Result{entry.value("genres", std::string{}), entry.value("source", std::string{})};
    } catch (const std::exception &e) {
        spdlog::warn("Ignoring unreadable genre cache: {}", e.what());
    }
}

void GenreLookup::write_cache(const json &j)
{
    std::error_code ec;
    std::filesystem::create_directories(cache_path().parent_path(), ec);
    const auto tmp = cache_path().string() + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out)
            return;
        out << j.dump();
    }
    std::filesystem::rename(tmp, cache_path(), ec);
}

void GenreLookup::publish(const std::string &track_id, const Result &result) const
{
    if (result.genres.empty())
        return;
    RuntimeInputs::publish(RuntimeInputIds::MusicGenre,
                           {{"track_id", track_id}, {"genres", result.genres}, {"source", result.source}},
                           std::chrono::seconds(45));
}

void GenreLookup::track_playing(const std::string &track_id, const std::string &title, const std::string &artist,
                                std::vector<std::string> artist_ids)
{
    if (track_id.empty())
        return;
    std::lock_guard lock(mtx_);
    if (!loaded_)
        load_cache();
    if (const auto it = cache_.find(track_id); it != cache_.end()) {
        publish(track_id, it->second);  // refresh the TTL while the track plays
        return;
    }
    if (in_flight_ == track_id || (pending_ && pending_->track_id == track_id))
        return;
    if (const auto retry = retry_after_.find(track_id);
        retry != retry_after_.end() && std::chrono::steady_clock::now() < retry->second)
        return;
    if (retry_after_.size() > 256)
        retry_after_.clear();
    pending_ = Pending{track_id, title, artist, std::move(artist_ids)};
    if (!thread_.joinable())
        thread_ = std::jthread([this](std::stop_token stop) { worker(stop); });
    cv_.notify_one();
}

void GenreLookup::stop()
{
    if (thread_.joinable()) {
        thread_.request_stop();
        cv_.notify_all();
        thread_.join();
    }
    std::lock_guard lock(mtx_);
    in_flight_.clear();
    pending_.reset();
    RuntimeInputs::set_available(RuntimeInputIds::MusicGenre, false);
}

void GenreLookup::worker(std::stop_token stop)
{
    while (!stop.stop_requested()) {
        Pending job;
        request_failed = false;
        {
            std::unique_lock lock(mtx_);
            cv_.wait(lock, stop, [this] { return pending_.has_value(); });
            if (stop.stop_requested())
                return;
            job = std::move(*pending_);
            pending_.reset();
            in_flight_ = job.track_id;
        }

        // Spotify's artist genres are the most specific; the free fallbacks
        // only run when it has none (Deezer is broad, GetSongBPM finer).
        std::vector<std::string> genres;
        std::set<std::string> seen;
        std::string source;
        const auto add = [&](const std::vector<std::string> &found, const char *name) {
            if (found.empty())
                return;
            source += (source.empty() ? "" : "+") + std::string(name);
            for (const auto &g : found)
                if (seen.insert(lower(g)).second)
                    genres.push_back(g);
        };
        add(spotify_genres(job.artist_ids), "spotify");
        if (stop.stop_requested())
            return;
        if (genres.empty()) {
            add(deezer_genres(job.title, job.artist, stop), "deezer");
            if (stop.stop_requested())
                return;
            add(getsongbpm_genres(job.title, job.artist, stop), "getsongbpm");
        }
        if (stop.stop_requested())
            return;

        Result result;
        for (const auto &g : genres) result.genres += (result.genres.empty() ? "" : ",") + g;
        result.source = source;
        spdlog::info("Genres for '{}' by '{}': {}", job.title, job.artist, result.genres.empty() ? "(none found)" : result.genres);

        json snapshot;
        {
            std::lock_guard lock(mtx_);
            in_flight_.clear();
            if (result.genres.empty() && request_failed) {
                // Transient failure: retry on a later poll instead of caching "no genres".
                retry_after_[job.track_id] = std::chrono::steady_clock::now() + std::chrono::minutes(2);
                continue;
            }
            if (cache_.size() >= MaxCacheEntries) {
                // Unordered, so this drops arbitrary old tracks; the cache is only an optimisation.
                auto it = cache_.begin();
                for (std::size_t n = 0; n < MaxCacheEntries / 5 && it != cache_.end(); ++n) it = cache_.erase(it);
            }
            cache_[job.track_id] = result;
            publish(job.track_id, result);
            snapshot = json::object();
            for (const auto &[id, r] : cache_) snapshot[id] = {{"genres", r.genres}, {"source", r.source}};
        }
        write_cache(snapshot);  // file I/O outside the lock
    }
}

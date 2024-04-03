#include "shared/spotify/spotify.h"
#include <cstdio>
#include <string>
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <iostream>

#include "shared/utils/shared.h"
#include "shared/utils/utils.h"

using namespace spdlog;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *out) {
    const size_t totalSize(size * nmemb);
    out->append((char *) contents, totalSize);
    return totalSize;
}

bool Spotify::refresh() {
    debug("Refreshing spotify token...");
    auto spAuth = config->get_spotify();

    if (!spAuth.has_auth()) {
        error("SpotifyScenes does not have auth");
        return false;
    }

    std::string url = "https://accounts.spotify.com/api/token";
    std::string post_fields = "grant_type=refresh_token&refresh_token=" + spAuth.refresh_token.value();

    std::string auth = client_id + ":" + client_secret;

    auto r = cpr::Post(cpr::Url{url},
                       cpr::Payload{
                               {"grant_type",    "refresh_token"},
                               {"refresh_token", spAuth.refresh_token.value()}
                       },
                       cpr::Authentication(client_id, client_secret, cpr::AuthMode::BASIC)
    );

    if (r.status_code != 200) {
        error("Could not refresh token: {}", r.text);
        return false;
    }

    debug("Saving to config...");
    return Spotify::save_resp_to_config(r.text);
}

bool Spotify::initialize() {
    auto spotify = config->get_spotify();
    auto any_empty = spotify.access_token->empty() || spotify.refresh_token->empty() || spotify.expires_at == 0;

    if (!any_empty) {
        debug("Curr time {}", GetTimeInMillis());
        bool is_expired = GetTimeInMillis() > spotify.expires_at;
        if (is_expired) {
            debug("Is expired. Refreshing...");
            return Spotify::refresh();
        }

        this->start_control_thread();
        return true;
    }

    auto curr = std::filesystem::current_path() / "spotify/authorize.js";


    printf("Path %s \n", curr.string().c_str());
    printf("Authorize at: http://10.6.0.23:8888/login \n");


    auto out = execute_process("node", {curr.string(), "8888"});
    if (!out.has_value())
        return false;

    auto res = Spotify::save_resp_to_config(out.value());
    if (!res)
        return false;

    this->start_control_thread();
    return true;
}

bool Spotify::save_resp_to_config(const std::string &json_resp) {
    debug("Saving '{}' to config...", json_resp);
    auto parsed = json::parse(json_resp);
    string access_token;
    uint expires_in_seconds;

    parsed.at("access_token").get_to(access_token);
    parsed.at("expires_in").get_to(expires_in_seconds);

    auto curr = config->get_spotify();
    string refresh_token = curr.refresh_token.value();

    if (parsed.contains("refresh_token")) {
        parsed.at("refresh_token").get_to(refresh_token);
    }

    [[maybe_unused]] tmillis_t expires_in = GetTimeInMillis() + expires_in_seconds * 1000;
    auto spotify_struct = ConfigData::SpotifyData(access_token, refresh_token, expires_in);

    config->set_spotify(spotify_struct);
    return config->save();
}

Spotify::Spotify() {
    auto id = std::getenv("SPOTIFY_CLIENT_ID");
    auto secret = std::getenv("SPOTIFY_CLIENT_SECRET");

    if (id && secret) {
        client_id = id;
        client_secret = secret;
    } else {
        error("No spotify client id or secret found in env");
        throw runtime_error("SPOTIFY_CLIENT_ID or SPOTIFY_CLIENT_SECRET not found in env");
    }
}

std::expected<optional<SpotifyState>, std::string> Spotify::inner_fetch_currently_playing() {
    auto res = Spotify::authenticated_get("https://api.spotify.com/v1/me/player/currently-playing?market=DE");
    if (!res.has_value()) {
        return unexpected(res.error());
    }

    if (!res.value().has_value()) {
        return nullopt;
    }

    if (!res->value().contains("item")) {
        warn("SpotifyScenes state does not contain item: {}", res->value().dump());
        return unexpected("Invalid server response: " + res->value().dump());
    }
    return SpotifyState(res->value());
}

std::expected<optional<json>, std::string> Spotify::authenticated_get(const string &url) {
    auto data = config->get_spotify();
    if (!data.has_auth()) {
        return unexpected("No auth data");
    }

    if (data.is_expired()) {
        if (!this->refresh()) {
            return unexpected("Could not refresh token");
        }

        data = config->get_spotify();
    }

    // Simple curl get to the given url with spotify bearer authentication
    CURL *curl = curl_easy_init();
    if (curl) {
        std::string readBuffer;


        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Set the Authorization header
        std::string auth_header = "Authorization: Bearer " + data.access_token.value();
        struct curl_slist *headers;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return unexpected("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (readBuffer.empty()) {
            return nullopt;
        }

        return json::parse(readBuffer);
    }

    return unexpected("Could not initialize curl!?");
}

void Spotify::start_control_thread() {
    thread c_thread{[this] {
        while (!this->should_terminate) {
            std::expected<optional<SpotifyState>, std::string> data = this->inner_fetch_currently_playing();
            if (!data.has_value()) {
                error("Could not get currently playing: {}", data.error());
                std::this_thread::sleep_for(std::chrono::seconds(15));

                continue;
            }

            auto opt_state = data.value();
            std::unique_lock<std::mutex> lock(mtx);
            if (this->currently_playing.has_value()) {
                this->last_playing.emplace(this->currently_playing.value());
            } else {
                this->last_playing.reset();
            }

            if (opt_state.has_value()) {
                SpotifyState state = std::move(opt_state.value());

                this->currently_playing.emplace(state);

                this->is_dirty = true;
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            } else {
                this->currently_playing.reset();

                this->is_dirty = true;
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::seconds(15));
            }

        }

    }};


    this->control_thread = std::move(c_thread);
}

void Spotify::terminate() {
    this->should_terminate.store(true);
    this->control_thread.join();
}

std::optional<SpotifyState> Spotify::get_currently_playing() {
    std::lock_guard lock(mtx);
    return this->currently_playing;
}

/**
 * Checks if the current track has changed from the previous call of this function
 */
bool Spotify::has_changed(bool update_dirty) {
    if (!this->is_dirty)
        return false;

    if (update_dirty)
        this->is_dirty = false;

    debug("Checking if has changed");
    std::lock_guard lock(mtx);
    if (this->last_playing.has_value() != this->currently_playing.has_value())
        return true;

    if(!this->last_playing.has_value() || !this->currently_playing.has_value())
        return true;

    return this->last_playing->get_track().get_id() != this->currently_playing->get_track().get_id();
}
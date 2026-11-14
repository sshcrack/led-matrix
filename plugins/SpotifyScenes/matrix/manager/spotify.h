#pragma once

#include <string>
#include "nlohmann/json.hpp"
#include <expected>
#include <thread>
#include <mutex>

#include "./state.h"

namespace Config {
class MainConfig;
}

class Spotify {
public:
    /// Store a token response (initial login or a re-login with new scopes).
    bool save_resp_to_config(const std::string& json_resp);

private:
    // The host's config (credentials, tokens, plugin keys); outlives this client.
    Config::MainConfig* config;

    std::expected<std::optional<nlohmann::json >, std::pair<std::string, std::optional<int>>> authenticated_get(const std::string& url, bool refresh = true);
    std::expected<std::optional<SpotifyState>, std::pair<std::string, std::optional<int>>> inner_fetch_currently_playing();

    std::mutex mtx;
    // Serialises token expiry checks/refreshes (control thread + genre lookup).
    std::mutex refresh_mtx;

    std::optional<SpotifyState> last_playing;
    std::optional<SpotifyState> currently_playing;
    std::thread control_thread;
    std::atomic<bool> should_terminate = false;

    std::atomic<bool> is_dirty = false;

    void busy_wait(int seconds);

public:
    std::atomic<bool> needs_reconnect = false;
    bool store_login(const std::string& response, bool pkce);
    bool configure(const std::string& id);

    explicit Spotify(Config::MainConfig& config, bool allow_missing_credentials = false);
    ~Spotify();  // Add destructor declaration

    void start_control_thread();
    bool initialize();
    bool refresh();
    void terminate();
    std::string get_client_id() const;
    std::string get_client_secret() const;
    bool is_configured() const { return !get_client_id().empty(); }
    bool has_auth() const;
    /// pluginConfigs.song_bpm_api_key, empty when not set.
    std::string song_bpm_api_key() const;
    std::optional<SpotifyState> get_currently_playing();
    void set_preview_currently_playing(nlohmann::json state_json);
    void clear_preview_currently_playing();

    bool has_changed(bool update_dirty);
};


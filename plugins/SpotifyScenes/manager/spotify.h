#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <expected>
#include <thread>

#include "./state.h"

class Spotify {
private:
    static bool save_resp_to_config(const std::string& json_resp);
    std::expected<std::optional<nlohmann::json >, std::pair<std::string, std::optional<int>>> authenticated_get(const std::string& url, bool refresh = true);
    std::expected<std::optional<SpotifyState>, std::pair<std::string, std::optional<int>>> inner_fetch_currently_playing();

    std::string client_id;
    std::string client_secret;
    std::mutex mtx;

    std::optional<SpotifyState> last_playing;
    std::optional<SpotifyState> currently_playing;
    std::thread control_thread;
    std::atomic<bool> should_terminate = false;

    std::atomic<bool> is_dirty = false;

    void busy_wait(int seconds);

public:
    explicit Spotify();
    ~Spotify();  // Add destructor declaration

    void start_control_thread();
    bool initialize();
    bool refresh();
    void terminate();
    std::optional<SpotifyState> get_currently_playing();

    bool has_changed(bool update_dirty);
};


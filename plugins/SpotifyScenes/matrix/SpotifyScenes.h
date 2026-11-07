#pragma once

#include "shared/matrix/plugin/main.h"
#include <shared/matrix/input_ids.h>
#include <mutex>
#include <chrono>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class SpotifyScenes : public BasicPlugin {
public:
    SpotifyScenes();

    vector<std::unique_ptr<SceneWrapper>> create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper>> create_image_providers() override;
    vector<std::unique_ptr<Previews::DataProvider>> create_preview_data_providers() override;
    [[nodiscard]] vector<std::string> get_runtime_input_ids() const override {
        return {std::string(RuntimeInputIds::SpotifyPlayback)};
    }

    std::optional<string> before_server_init() override;
    std::optional<string> after_server_init() override;
    std::optional<string> pre_exit() override;

    std::unique_ptr<router_t> register_routes(std::unique_ptr<router_t> router) override;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }

private:
    static string generate_random_string(size_t length);
    std::mutex setup_mutex;
    string pending_state;
    string pending_verifier; // PKCE code_verifier for the pending login (never leaves the Pi)
    string pending_redirect; // redirect_uri the pending login was started with (must match at token exchange)
    std::chrono::steady_clock::time_point state_expires;
    restinio::request_handling_status_t complete_login(
        const restinio::request_handle_t& req, const string& query, bool browser);
};
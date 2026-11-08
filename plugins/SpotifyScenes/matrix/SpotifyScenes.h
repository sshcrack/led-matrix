#pragma once

#include "shared/matrix/plugin/main.h"
#include <shared/matrix/input_ids.h>
#include <mutex>
#include "manager/login_attempt.h"

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
    // setup_mutex serializes the setup routes (credentials/connect/complete, including the token exchange);
    // login_mutex only guards `login`, so /spotify/status never waits for a slow exchange. Order: setup, then login.
    std::mutex setup_mutex;
    std::mutex login_mutex;
    SpotifyLogin::Attempt login;
    restinio::request_handling_status_t complete_login(
        const restinio::request_handle_t& req, const string& query, bool browser);
};
#include "SpotifyScenes.h"
#include "SpotifyPreviewProvider.h"
#include <random>
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/plugin_registry.h"
#include "manager/shared_spotify.h"
#include "scenes/CoverOnlyScene.h"
#include "spdlog/spdlog.h"
#include "cpr/cpr.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/media_artwork_state.h"
#include "shared/matrix/preview.h"
#include "shared/matrix/execution_mode.h"

using namespace Scenes;

REGISTER_PLUGIN_CUSTOM_DESTROY(SpotifyScenes, SpotifyScenes, delete c; delete spotify;)

vector<std::unique_ptr<ImageProviderWrapper> >
SpotifyScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<Previews::DataProvider>>
SpotifyScenes::create_preview_data_providers() {
    vector<std::unique_ptr<Previews::DataProvider>> providers;
    providers.push_back(std::make_unique<SpotifyPreviewProvider>());
    return providers;
}

vector<std::unique_ptr<SceneWrapper>> SpotifyScenes::create_scenes() {
    
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<CoverOnlySceneWrapper>());
    return scenes;
}

std::optional<string> SpotifyScenes::before_server_init() {

    // The /spotify/* routes dereference `spotify`, and the daemon starts
    // accepting requests before after_server_init(). Construct the client
    // here so a request arriving during startup never sees a null pointer.
    if (!spotify)
        spotify = new Spotify(SceneExecution::is_headless_fixture_host());
    return std::nullopt;
}

std::optional<string> SpotifyScenes::after_server_init() {
    if (!spotify)
        return "SpotifyScenes: before_server_init() did not create the Spotify client";

    spdlog::debug("Initializing SpotifyScenes");

    // Start polling without blocking startup; setup happens in the web UI.
    if (!SceneExecution::is_headless_fixture_host())
        spotify->initialize();
    PluginRegistry::set("spotify", spotify);

    if (!SceneExecution::is_headless_fixture_host())
        config->save();
    return std::nullopt;
}

std::optional<string> SpotifyScenes::pre_exit() {
    MediaArtworkState::clear();
    return BasicPlugin::pre_exit();
}

std::unique_ptr<router_t> SpotifyScenes::
register_routes(std::unique_ptr<router_t> router) {

    const string redirect_uri = "http://127.0.0.1:8080/spotify/callback";
    router->http_get("/spotify/status", [redirect_uri](const auto& req, auto) {
        return Server::reply_with_json(req, {
            {"configured", spotify->is_configured()},
            {"connected", config->get_spotify().has_auth() && !spotify->needs_reconnect.load()},
            {"client_id", spotify->get_client_id()}, {"redirect_uri", redirect_uri}
        });
    });
    router->http_post("/spotify/credentials", [this](const auto& req, auto) {
        std::lock_guard lock(setup_mutex);
        try {
            const auto body = json::parse(req->body());
            const auto id = body.at("client_id").template get<string>();
            const auto secret = body.value("client_secret", "");
            const auto valid = [](const string& value) {
                return value.size() == 32 && value.find_first_not_of("0123456789abcdefABCDEF") == string::npos;
            };
            if (!valid(id) || (!secret.empty() && !valid(secret)) ||
                (secret.empty() && spotify->get_client_secret().empty()))
                return Server::reply_with_error(req, "Enter the Client ID and Client secret from your Spotify app settings.");
            pending_state.clear();
            if (!spotify->configure(id, secret))
                return Server::reply_with_error(req, "Could not save Spotify settings. Check the config directory is writable.");
            return Server::reply_success(req);
        } catch (const std::exception&) {
            return Server::reply_with_error(req, "Enter valid Spotify app credentials.");
        }
    });
    router->http_post("/spotify/connect", [this](const auto& req, auto) {
        std::lock_guard lock(setup_mutex);
        if (!spotify->is_configured())
            return Server::reply_with_error(req, "Save your Spotify app credentials first.");
        pending_state = generate_random_string(48);
        state_expires = std::chrono::steady_clock::now() + std::chrono::minutes(10);
        const string url = "https://accounts.spotify.com/authorize?response_type=code&client_id=" +
            spotify->get_client_id() + "&scope=user-read-playback-state%20user-read-currently-playing%20user-top-read" +
            "&redirect_uri=http%3A%2F%2F127.0.0.1%3A8080%2Fspotify%2Fcallback&state=" + pending_state;
        return Server::reply_with_json(req, {{"url", url}});
    });
    router->http_post("/spotify/complete", [this](const auto& req, auto) {
        try {
            const auto body = json::parse(req->body());
            const auto url = body.at("callback_url").template get<string>();
            const string prefix = "http://127.0.0.1:8080/spotify/callback?";
            if (!url.starts_with(prefix))
                return Server::reply_with_error(req, "Paste the full address starting with http://127.0.0.1:8080/spotify/callback?");
            return complete_login(req, url.substr(prefix.size()), false);
        } catch (const std::exception&) {
            return Server::reply_with_error(req, "Could not read the returned address. Copy the full address from the Spotify tab.");
        }
    });
    router->http_get("/spotify/callback", [this](const auto& req, auto) {
        return complete_login(req, std::string(req->header().query()), true);
    });
    return std::move(router);
}

restinio::request_handling_status_t SpotifyScenes::complete_login(
    const restinio::request_handle_t& req, const string& query, bool browser) {
    std::lock_guard lock(setup_mutex);
    try {
        const auto qp = restinio::parse_query(query);
        const string state = qp.has("state") ? string(qp["state"]) : "";
        if (state.empty() || pending_state.empty() || state != pending_state ||
            std::chrono::steady_clock::now() > state_expires)
            return Server::reply_with_error(req, "This Spotify login has expired or does not match. Choose Connect Spotify and try again.");
        pending_state.clear(); // one-time use, including declined and failed attempts
        if (qp.has("error"))
            return Server::reply_with_error(req, "Spotify access was declined. Choose Connect Spotify to try again.");
        if (!qp.has("code") || qp["code"].empty())
            return Server::reply_with_error(req, "Spotify did not return an authorization code. Try connecting again.");
        auto res = cpr::Post(cpr::Url{"https://accounts.spotify.com/api/token"},
            cpr::Payload{{"grant_type", "authorization_code"}, {"code", string(qp["code"])},
                         {"redirect_uri", "http://127.0.0.1:8080/spotify/callback"}},
            cpr::Authentication{spotify->get_client_id(), spotify->get_client_secret(), cpr::AuthMode::BASIC},
            cpr::Timeout{15000});
        if (res.status_code != 200)
            return Server::reply_with_error(req, "Spotify could not connect. Check your app credentials and redirect URI, then connect again.");
        if (!spotify->store_login(res.text))
            return Server::reply_with_error(req, "Could not save the Spotify login. Check the config directory is writable.");
        if (browser) {
            return req->create_response().append_header(restinio::http_field::content_type, "text/html; charset=utf-8")
                .append_header("Cache-Control", "no-store")
                .set_body("<!doctype html><meta name=viewport content='width=device-width'><title>Spotify connected</title><h1>Spotify connected</h1><p>You can close this tab and return to your matrix.</p>").done();
        }
        return Server::reply_success(req);
    } catch (const std::exception&) {
        return Server::reply_with_error(req, "Spotify returned an invalid response. Choose Connect Spotify to try again.");
    }
}

string SpotifyScenes::generate_random_string(size_t length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rng;
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    string result;
    result.resize(length);

    for (size_t i = 0; i < length; i++) {
        result[i] = charset[dist(rng)];
    }

    return result;
}

SpotifyScenes::SpotifyScenes() = default;

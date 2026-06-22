#include "SpotifyScenes.h"
#include <random>
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/plugin_registry.h"
#include "manager/shared_spotify.h"
#include "scenes/CoverOnlyScene.h"
#include "spdlog/spdlog.h"
#include "cpr/cpr.h"
#include "shared/matrix/server/server_utils.h"

using namespace Scenes;

extern "C" PLUGIN_EXPORT SpotifyScenes *createSpotifyScenes() {
    return new SpotifyScenes();
}

extern "C" PLUGIN_EXPORT void destroySpotifyScenes(SpotifyScenes *c) {
    delete c;
    delete spotify; // The destructor will handle termination
}

vector<std::unique_ptr<ImageProviderWrapper> >
SpotifyScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> SpotifyScenes::create_scenes() {
    if(is_disabled)
        return {};
    
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<CoverOnlySceneWrapper>());
    return scenes;
}

std::optional<string> SpotifyScenes::after_server_init() {
    if(is_disabled)
        return std::nullopt;

    spdlog::debug("Initializing SpotifyScenes");

    spotify = new Spotify();
    spotify->initialize();
    PluginRegistry::set("spotify", spotify);

    config->save();
    return std::nullopt;
}

std::unique_ptr<router_t> SpotifyScenes::
register_routes(std::unique_ptr<router_t> router) {
    if(is_disabled)
        return std::move(router);

    const string redirect_uri = "http://127.0.0.1:8080/spotify/callback";

    router->http_get("/spotify/login",
                     [this, redirect_uri](const restinio::request_handle_t &req, auto) {
                         const string state = generate_random_string(16);
                         const string scope = "user-read-playback-state user-read-currently-playing";

                         std::stringstream auth_url;
                         auth_url << "https://accounts.spotify.com/authorize?"
                                 << "response_type=code"
                                 << "&client_id=" << spotify->get_client_id()
                                 << "&scope=" << scope
                                 << "&redirect_uri=" << redirect_uri
                                 << "&state=" << state;

                         auto response = req->create_response(restinio::status_temporary_redirect())
                                 .append_header(restinio::http_field::location, auth_url.str());
                         Server::add_cors_headers(response);
                         return response.done();
                     });

    router->http_get("/spotify/callback",
                     [this, redirect_uri](const restinio::request_handle_t &req, auto) {
                         const auto qp = restinio::parse_query(req->header().query());
                         const auto code = !qp.has("code") ? "" : std::string{qp["code"]};
                         const auto state = !qp.has("state") ? "" : std::string{qp["state"]};

                         if (state.empty()) {
                             auto response = req->create_response(restinio::status_bad_request())
                                     .set_body("State mismatch");
                             Server::add_cors_headers(response);
                             return response.done();
                         }

                         if (!code.empty()) {
                             spdlog::debug("Using {} and {}", spotify->get_client_id(), spotify->get_client_secret());
                             auto res = cpr::Post(cpr::Url{"https://accounts.spotify.com/api/token"},
                                                  cpr::Payload{
                                                      {"grant_type", "authorization_code"},
                                                      {"code", code},
                                                      {"redirect_uri", redirect_uri}
                                                  },
                                                  cpr::Authentication{
                                                      spotify->get_client_id(),
                                                      spotify->get_client_secret(),
                                                      cpr::AuthMode::BASIC
                                                  });

                             spotify->spotify_callback = res.text;

                             return Server::reply_with_json(req, {{"success", true}});
                         }

                         auto response = req->create_response(restinio::status_bad_request())
                                 .set_body("Missing code parameter");
                         Server::add_cors_headers(response);
                         return response.done();
                     });

    return std::move(router);
}

string SpotifyScenes::generate_random_string(size_t length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    string result;
    result.resize(length);

    for (size_t i = 0; i < length; i++) {
        result[i] = charset[dist(rng)];
    }

    return result;
}

SpotifyScenes::SpotifyScenes() {
    auto id = std::getenv("SPOTIFY_CLIENT_ID");
    auto secret = std::getenv("SPOTIFY_CLIENT_SECRET");

    is_disabled = !id || !secret;
    if (is_disabled)
        spdlog::warn("SpotifyScenes is disabled: SPOTIFY_CLIENT_ID or SPOTIFY_CLIENT_SECRET not found in the environment. The plugin will be disabled");
};

#include "SpotifyScenes.h"
#include "shared/utils/shared.h"
#include "manager/shared_spotify.h"
#include "scenes/CoverOnlyScene.h"
#include "spdlog/spdlog.h"
#include <cpr/cpr.h>

using namespace Scenes;

extern "C" [[maybe_unused]] SpotifyScenes *createSpotifyScenes() {
    return new SpotifyScenes();
}

extern "C" [[maybe_unused]] void destroySpotifyScenes(SpotifyScenes *c) {
    delete spotify; // The destructor will handle termination
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)> >
SpotifyScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> > SpotifyScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> >();
    scenes.push_back({
        new CoverOnlySceneWrapper(), [](SceneWrapper *scene) {
            delete scene;
        }
    });

    return scenes;
}

std::optional<string> SpotifyScenes::post_init() {
    spdlog::info("Initializing SpotifyScenes");

    spotify = new Spotify();
    spotify->initialize();

    config->save();
    return BasicPlugin::post_init();
}

std::unique_ptr<router_t> SpotifyScenes::
register_routes(std::unique_ptr<router_t> router) {
    const string redirect_uri = "http://127.0.0.1:8080/spotify/callback";

    router->http_get("/spotify/login",
                     [this, redirect_uri](const restinio::request_handle_t &req, auto params) {
                         const string state = generate_random_string(16);
                         const string scope = "user-read-playback-state user-read-currently-playing";

                         std::stringstream auth_url;
                         auth_url << "https://accounts.spotify.com/authorize?"
                                 << "response_type=code"
                                 << "&client_id=" << spotify->get_client_id()
                                 << "&scope=" << scope
                                 << "&redirect_uri=" << redirect_uri
                                 << "&state=" << state;

                         return req->create_response(restinio::status_temporary_redirect())
                                 .append_header(restinio::http_field::location, auth_url.str())
                                 .done();
                     });

    router->http_get("/spotify/callback",
                     [this, redirect_uri](const restinio::request_handle_t &req, auto qp) {
                         const auto code = std::string{qp.get_param("code").value_or("")};
                         const auto state = std::string{qp.get_param("state").value_or("")};

                         if (state.empty()) {
                             return req->create_response(restinio::status_bad_request())
                                     .set_body("State mismatch")
                                     .done();
                         }

                         if (!code.empty()) {
                             auto res = cpr::Post(cpr::Url{"https://accounts.spotify.com/api/token"},
                                                  cpr::Payload{
                                                      {"grant_type", "authorization_code"},
                                                      {"code", code},
                                                      {"redirect_uri", redirect_uri}
                                                  },
                                                  cpr::Header{
                                                      {
                                                          "Authorization",
                                                          "Basic " + spotify->get_client_id() + ":" +
                                                          spotify->get_client_secret()
                                                      }
                                                  });

                             spotify->spotify_callback = res.text;
                             spotify->had_spotify_callback = true;

                             return req->create_response(restinio::status_ok())
                                     .set_body("{\"success\": true}")
                                     .done();
                         }

                         return req->create_response(restinio::status_bad_request())
                                 .set_body("Missing code parameter")
                                 .done();
                     });

    return std::move(router);
}

string SpotifyScenes::generate_random_string(size_t length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    string result;
    result.resize(length);

    for (size_t i = 0; i < length; i++) {
        result[i] = charset[rand() % (sizeof(charset) - 1)];
    }

    return result;
}

SpotifyScenes::SpotifyScenes() = default;

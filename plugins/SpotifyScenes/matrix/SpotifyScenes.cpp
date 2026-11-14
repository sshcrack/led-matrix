#include "SpotifyScenes.h"

#include <random>

#include "SpotifyPreviewProvider.h"
#include "cpr/cpr.h"
#include "manager/pkce.h"
#include "manager/shared_spotify.h"
#include "scenes/CoverEqualizerScene.h"
#include "scenes/CoverOnlyScene.h"
#include "scenes/HalftoneScene.h"
#include "scenes/VinylScene.h"
#include "shared/matrix/execution_mode.h"
#include "shared/matrix/media_artwork_state.h"
#include "shared/matrix/plugin_registry.h"
#include "shared/matrix/preview.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"

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
    vector<std::unique_ptr<SceneWrapper>> created;
    created.push_back(std::make_unique<CoverOnlySceneWrapper>());
    created.push_back(std::make_unique<VinylSceneWrapper>());
    created.push_back(std::make_unique<CoverEqualizerSceneWrapper>());
    created.push_back(std::make_unique<HalftoneSceneWrapper>());
    return created;
}

std::optional<string> SpotifyScenes::before_server_init() {

    // The /spotify/* routes dereference `spotify`, and the daemon starts
    // accepting requests before after_server_init(). Construct the client
    // here so a request arriving during startup never sees a null pointer.
    auto* config = host_config();
    if (!config)
        return "SpotifyScenes: the host did not provide a config";
    if (!spotify)
        spotify = new Spotify(*config, SceneExecution::is_headless_fixture_host());
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
        host_config()->save();
    return std::nullopt;
}

std::optional<string> SpotifyScenes::pre_exit() {
    MediaArtworkState::clear();
    return BasicPlugin::pre_exit();
}

namespace {
// Spotify only allows https redirects (or loopback), so sign-in returns to a small static relay page that
// sends the browser back to this matrix's own address, carried in `state` as "<nonce>.<base64url(origin)>".
// Self-hosters can point LED_MATRIX_SPOTIFY_REDIRECT_URI at their own copy of hosted/spotify-auth.
constexpr const char* kDefaultRelayRedirect = "https://spotify-auth.sshcrack.me/callback";
constexpr const char* kLoopbackRedirect = "http://127.0.0.1:8080/spotify/callback";

string relay_redirect_uri() {
    const char* env = std::getenv("LED_MATRIX_SPOTIFY_REDIRECT_URI");
    return env && *env ? string(env) : string(kDefaultRelayRedirect);
}

// scheme://host[:port] only: no path, credentials or odd characters.
bool is_plain_origin(const string& o) {
    const size_t start = o.starts_with("https://") ? 8 : o.starts_with("http://") ? 7 : 0;
    if (start == 0 || o.size() <= start || o.size() > 255) return false;
    return o.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:[]", start) == string::npos;
}

string percent_encode(const string& in) {
    static constexpr char hex[] = "0123456789ABCDEF";
    string out;
    for (const unsigned char c : in) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
} // namespace

std::unique_ptr<router_t> SpotifyScenes::
register_routes(std::unique_ptr<router_t> router) {

    router->http_get("/spotify/status", [this](const auto& req, auto) {
        const string redirect_uri = relay_redirect_uri();
        SpotifyLogin::Snapshot attempt;
        {
            std::lock_guard lock(login_mutex);
            attempt = login.snapshot(SpotifyLogin::Clock::now());
        }
        json login_json = {{"attempt", attempt.attempt}, {"phase", SpotifyLogin::to_string(attempt.phase)}};
        if (!attempt.error.empty()) login_json["error"] = attempt.error;
        return Server::reply_with_json(req, {{"configured", spotify->is_configured()},
                                             {"connected", spotify->has_auth() && !spotify->needs_reconnect.load()},
                                             {"client_id", spotify->get_client_id()},
                                             {"redirect_uri", redirect_uri},
                                             {"fallback_redirect_uri", kLoopbackRedirect},
                                             // The latest sign-in attempt, so the browser that started it can follow it to its end.
                                             {"login", std::move(login_json)}});
    });
    router->http_post("/spotify/credentials", [this](const auto& req, auto) {
        std::lock_guard lock(setup_mutex);
        try {
            const auto body = json::parse(req->body());
            const auto id = body.at("client_id").template get<string>();
            const auto valid = [](const string& value) {
                return value.size() == 32 && value.find_first_not_of("0123456789abcdefABCDEF") == string::npos;
            };
            // Sign-in uses PKCE, so only the Client ID is needed; a Client secret sent by older UIs is ignored.
            if (!valid(id))
                return Server::reply_with_error(req, "Enter the Client ID from your Spotify app settings.");
            {
                std::lock_guard login_lock(login_mutex);
                login.cancel();
            }
            if (!spotify->configure(id))
                return Server::reply_with_error(req, "Could not save Spotify settings. Check the config directory is writable.");
            return Server::reply_success(req);
        } catch (const std::exception&) {
            return Server::reply_with_error(req, "Enter valid Spotify app credentials.");
        }
    });
    router->http_post("/spotify/connect", [this](const auto& req, auto) {
        std::lock_guard lock(setup_mutex);
        if (!spotify->is_configured())
            return Server::reply_with_error(req, "Save your Spotify Client ID first.");
        string origin;
        try {
            const auto body = json::parse(req->body().empty() ? "{}" : req->body());
            origin = body.value("origin", "");
        } catch (const std::exception&) {}
        SpotifyLogin::Pending pending;
        pending.state = generate_random_string(48);
        if (is_plain_origin(origin)) {
            // Return through the hosted relay to the address this browser used to reach the matrix.
            pending.state += "." + Pkce::base64url(origin);
            pending.redirect = relay_redirect_uri();
        } else {
            pending.redirect = kLoopbackRedirect; // no origin supplied: legacy same-device flow
        }
        pending.verifier = generate_random_string(64); // PKCE: stays on the Pi, only its SHA-256 is sent
        const string url = "https://accounts.spotify.com/authorize?response_type=code&client_id=" +
            spotify->get_client_id() + "&scope=user-read-playback-state%20user-read-currently-playing%20user-top-read" +
            "&redirect_uri=" + percent_encode(pending.redirect) + "&code_challenge_method=S256&code_challenge=" +
            Pkce::challenge(pending.verifier) + "&state=" + pending.state;
        std::uint64_t attempt;
        {
            std::lock_guard login_lock(login_mutex);
            attempt = login.begin(std::move(pending), SpotifyLogin::Clock::now());
        }
        return Server::reply_with_json(req, {{"url", url}, {"attempt", attempt}});
    });
    router->http_post("/spotify/complete", [this](const auto& req, auto) {
        try {
            const auto body = json::parse(req->body());
            const auto url = body.at("callback_url").template get<string>();
            const auto q = url.find('?');
            const auto base = url.substr(0, q);
            if (q == string::npos || (base != kLoopbackRedirect && base != relay_redirect_uri() &&
                                      !base.ends_with("/spotify/callback")))
                return Server::reply_with_error(req, "Paste the full address of the page you were sent to, including everything after the ?");
            return complete_login(req, url.substr(q + 1), false);
        } catch (const std::exception&) {
            return Server::reply_with_error(req, "Could not read the returned address. Copy the full address from the Spotify tab.");
        }
    });
    router->http_get("/spotify/callback", [this](const auto& req, auto) {
        return complete_login(req, std::string(req->header().query()), true);
    });
    return router;
}

restinio::request_handling_status_t SpotifyScenes::complete_login(
    const restinio::request_handle_t& req, const string& query, bool browser) {
    // The /spotify/callback tab is a person looking at a page, so it gets readable HTML; the pasted-address
    // route (/spotify/complete) is the web UI and gets JSON.
    const auto page = [&](restinio::http_status_line_t status, std::string_view title, std::string_view text) {
        return req->create_response(std::move(status))
            .append_header(restinio::http_field::content_type, "text/html; charset=utf-8")
            .append_header("Cache-Control", "no-store")
            .set_body(std::string("<!doctype html><meta name=viewport content='width=device-width'><title>") +
                      string(title) + "</title><h1>" + string(title) + "</h1><p>" + string(text) +
                      "</p><p>You can close this tab, or <a href=\"/\">return to your matrix</a>.</p>")
            .done();
    };
    const auto reject = [&](const string& message) {
        return browser ? page(restinio::status_bad_request(), "Spotify not connected", message)
                       : Server::reply_with_error(req, message);
    };

    std::lock_guard lock(setup_mutex);
    // Every exit after take() must record the attempt's outcome (fail/succeed): the browser waits on it.
    const auto failed = [&](const string& message) {
        { std::lock_guard login_lock(login_mutex); login.fail(message); }
        return reject(message);
    };
    try {
        const auto qp = restinio::parse_query(query);
        const string state = qp.has("state") ? string(qp["state"]) : "";
        auto taken = [&] {
            std::lock_guard login_lock(login_mutex);
            return login.take(state, SpotifyLogin::Clock::now());
        }();
        if (!taken) return reject(taken.error());
        if (qp.has("error"))
            return failed("Spotify access was declined. Choose Connect Spotify to try again.");
        if (!qp.has("code") || qp["code"].empty())
            return failed("Spotify did not return an authorization code. Try connecting again.");
        auto res = cpr::Post(cpr::Url{"https://accounts.spotify.com/api/token"},
            cpr::Payload{{"grant_type", "authorization_code"}, {"code", string(qp["code"])},
                         {"redirect_uri", taken->redirect}, {"client_id", spotify->get_client_id()},
                         {"code_verifier", taken->verifier}},
            cpr::Timeout{15000});
        if (res.status_code != 200)
            return failed("Spotify could not connect. Check your Client ID and redirect URI, then connect again.");
        if (!spotify->store_login(res.text, true))
            return failed("Could not save the Spotify login. Check the config directory is writable.");
        { std::lock_guard login_lock(login_mutex); login.succeed(); }
        if (browser) return page(restinio::status_ok(), "Spotify connected", "Your matrix can now show what is playing.");
        return Server::reply_success(req);
    } catch (const std::exception&) {
        return failed("Spotify returned an invalid response. Choose Connect Spotify to try again.");
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

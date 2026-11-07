#include "server/request_guard.h"
#include "shared/matrix/utils/image_fetch.h"

#include <iostream>
#include <string>

using namespace Server::RequestGuard;

namespace {
    int failures = 0;

    void expect(const char *name, const Verdict actual, const Verdict expected) {
        if (actual != expected) {
            std::cerr << "FAIL: " << name << " (got " << static_cast<int>(actual)
                      << ", expected " << static_cast<int>(expected) << ")\n";
            ++failures;
        }
    }

    RequestView view(std::string_view path, std::string_view host) {
        RequestView v;
        v.path = path;
        v.host = host;
        return v;
    }
}

int main() {
    const Policy open{};

    auto same_origin = view("/api/update/install", "ledmat:8080");
    same_origin.origin = "http://ledmat:8080";
    same_origin.has_origin = true;
    expect("same origin allowed", evaluate(open, same_origin), Verdict::Allow);

    auto default_port = view("/set_active", "LEDMAT");
    default_port.origin = "http://ledmat";
    default_port.has_origin = true;
    expect("default port and case normalised", evaluate(open, default_port), Verdict::Allow);

    auto desktop_ws = view("/desktopWebsocket", "192.168.1.5:8080");
    desktop_ws.origin = "ws://192.168.1.5:8080";
    desktop_ws.has_origin = true;
    expect("ixwebsocket origin allowed", evaluate(open, desktop_ws), Verdict::Allow);

    auto cross = view("/api/update/install", "ledmat:8080");
    cross.origin = "https://evil.example";
    cross.has_origin = true;
    expect("cross origin rejected", evaluate(open, cross), Verdict::CrossOrigin);

    auto null_origin = view("/preset", "ledmat:8080");
    null_origin.origin = "null";
    null_origin.has_origin = true;
    expect("opaque origin rejected", evaluate(open, null_origin), Verdict::CrossOrigin);

    auto img_get = view("/set_active", "ledmat:8080");
    img_get.sec_fetch_site = "cross-site";
    expect("cross-site navigation rejected", evaluate(open, img_get), Verdict::CrossOrigin);

    expect("non-browser client allowed", evaluate(open, view("/list_presets", "ledmat:8080")), Verdict::Allow);

    Policy dev{};
    dev.allowed_origins = {"http://localhost:5173"};
    auto vite = view("/list_presets", "ledmat:8080");
    vite.origin = "http://localhost:5173";
    vite.has_origin = true;
    expect("allowlisted origin allowed", evaluate(dev, vite), Verdict::Allow);

    Policy locked{};
    locked.token = "s3cret";
    expect("missing token rejected", evaluate(locked, view("/list_presets", "ledmat")), Verdict::Unauthorized);
    expect("web ui public", evaluate(locked, view("/web/index.html", "ledmat")), Verdict::Allow);
    expect("root public", evaluate(locked, view("/", "ledmat")), Verdict::Allow);

    auto callback = view("/spotify/callback", "127.0.0.1:8080");
    callback.sec_fetch_site = "cross-site";
    expect("OAuth callback uses its own state authentication", evaluate(locked, callback), Verdict::Allow);
    expect("Spotify credentials require token", evaluate(locked, view("/spotify/credentials", "ledmat")), Verdict::Unauthorized);
    expect("Spotify completion requires token", evaluate(locked, view("/spotify/complete", "ledmat")), Verdict::Unauthorized);
    auto bearer = view("/list_presets", "ledmat");
    bearer.authorization = "Bearer s3cret";
    expect("bearer token accepted", evaluate(locked, bearer), Verdict::Allow);

    auto wrong = view("/list_presets", "ledmat");
    wrong.authorization = "Bearer s3creX";
    expect("wrong bearer rejected", evaluate(locked, wrong), Verdict::Unauthorized);

    auto header = view("/list_presets", "ledmat");
    header.api_token_header = "s3cret";
    expect("x-api-token accepted", evaluate(locked, header), Verdict::Allow);

    auto query = view("/live_frame", "ledmat");
    query.query_token = "s3cret";
    expect("query token accepted", evaluate(locked, query), Verdict::Allow);

    auto cookie = view("/scene_preview", "ledmat");
    cookie.cookie_header = "theme=dark; led_matrix_token=s3cret";
    expect("cookie token accepted", evaluate(locked, cookie), Verdict::Allow);

    auto bad_cookie = view("/scene_preview", "ledmat");
    bad_cookie.cookie_header = "led_matrix_token=nope";
    expect("wrong cookie rejected", evaluate(locked, bad_cookie), Verdict::Unauthorized);

    auto cross_with_token = cross;
    cross_with_token.authorization = "Bearer s3cret";
    expect("token does not bypass origin check", evaluate(locked, cross_with_token), Verdict::CrossOrigin);

    const std::pair<const char *, bool> addresses[] = {
        {"127.0.0.1", true}, {"0.0.0.0", true}, {"169.254.169.254", true}, {"::1", true}, {"::", true},
        {"fe80::1", true}, {"::ffff:127.0.0.1", true}, {"not-an-ip", true},
        {"10.0.0.5", false}, {"192.168.1.20", false}, {"8.8.8.8", false}, {"::ffff:8.8.8.8", false},
        {"2606:4700::1", false},
    };
    for (const auto &[address, internal] : addresses) {
        if (utils::is_internal_address(address) != internal) {
            std::cerr << "FAIL: is_internal_address(" << address << ") != " << internal << "\n";
            ++failures;
        }
    }

    if (failures == 0) std::cout << "request_guard_smoke: all checks passed\n";
    return failures == 0 ? 0 : 1;
}

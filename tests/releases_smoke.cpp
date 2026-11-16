// Releases (shared/common): the one GitHub release module behind the Pi updater, the desktop
// updater and GET /api/update/releases. Fixture JSON and an injected transport, no network.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "shared/common/Releases.h"

namespace {
int failures = 0;

void check(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

using nlohmann::json;

const std::string kDeb = "https://github.com/sshcrack/led-matrix/releases/download/v1.20.0/led-matrix-1.20.0_arm64.deb";
const std::string kDigest = "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";  // "abc"

json asset(const std::string& name, const std::string& url, json digest = nullptr, json size = 1234)
{
    return {{"name", name}, {"browser_download_url", url}, {"digest", std::move(digest)}, {"size", std::move(size)}};
}

json release(const std::string& tag, json assets)
{
    return {{"tag_name", tag},
            {"name", "Release " + tag},
            {"body", "notes"},
            {"prerelease", false},
            {"draft", false},
            {"html_url", "https://github.com/sshcrack/led-matrix/releases/tag/" + tag},
            {"published_at", "2026-11-01T10:00:00Z"},
            {"assets", std::move(assets)}};
}

json standard_release()
{
    return release(
        "v1.20.0",
        json::array({
            asset("led-matrix-desktop-1.20.0-Linux.tar.gz",
                  "https://github.com/sshcrack/led-matrix/releases/download/v1.20.0/led-matrix-desktop-1.20.0-Linux.tar.gz", "sha256:aa"),
            asset("led-matrix-1.20.0_arm64.tar.gz",
                  "https://github.com/sshcrack/led-matrix/releases/download/v1.20.0/led-matrix-1.20.0_arm64.tar.gz", "sha256:bb"),
            asset("led-matrix-1.20.0_arm64.deb", kDeb, kDigest),
        }));
}

struct Recorder {
    std::vector<std::string> urls;
    std::vector<Releases::FetchOptions> options;
    Releases::HttpResponse response;
    Releases::HttpGet transport()
    {
        return [this](const std::string& url, const Releases::FetchOptions& o) {
            urls.push_back(url);
            options.push_back(o);
            return response;
        };
    }
};

void test_status()
{
    using S = Releases::UpdateStatus;
    check(std::string(Releases::status_name(S::DOWNLOADING)) == "downloading", "status name");
    for (int code = 0; code <= 6; ++code) {
        const auto status = Releases::status_from_code(code);
        check(status && static_cast<int>(*status) == code, "legacy code " + std::to_string(code) + " stays stable");
        check(status && Releases::status_from_name(Releases::status_name(*status)) == status,
              "name round-trips for " + std::to_string(code));
    }
    check(!Releases::status_from_code(7) && !Releases::status_from_code(-1), "out-of-range code is rejected");
    check(!Releases::status_from_name("Downloading") && !Releases::status_from_name(""), "names are exact");
    check(Releases::is_busy(S::CHECKING) && Releases::is_busy(S::DOWNLOADING) && Releases::is_busy(S::INSTALLING), "busy states");
    check(!Releases::is_busy(S::IDLE) && !Releases::is_busy(S::ERROR) && !Releases::is_busy(S::SUCCESS) && !Releases::is_busy(S::DISABLED),
          "idle states are not busy");

    json out;
    Releases::write_status(out, S::INSTALLING);
    check(out["status"] == 3 && out["status_name"] == "installing", "write_status sends the legacy int and the name");
    check(Releases::read_status(out) == S::INSTALLING, "read_status reads what write_status wrote");
    check(Releases::read_status(json{{"status", 2}}) == S::DOWNLOADING, "an older Pi sends only the int");
    check(Releases::read_status(json{{"status_name", "error"}}) == S::ERROR, "a newer Pi may send only the name");
    check(Releases::read_status(json{{"status", 1}, {"status_name", "installing"}}) == S::INSTALLING, "the name wins over the int");
    check(Releases::read_status(json{{"status", 2}, {"status_name", "from-the-future"}}) == S::DOWNLOADING,
          "an unknown name falls back to the int");
    check(!Releases::read_status(json{{"status_name", "from-the-future"}}), "an unknown name alone is unknown");
    check(!Releases::read_status(json{{"status", nullptr}}) && !Releases::read_status(json::array()) &&
              !Releases::read_status(json{{"status", 99}}),
          "garbage is unknown");
}

void test_parse()
{
    const auto parsed = Releases::parse_release(standard_release());
    check(parsed.has_value(), "standard release parses");
    if (parsed) {
        check(parsed->tag == "v1.20.0" && parsed->version == Common::Version::fromString("1.20.0"), "tag and version");
        check(parsed->assets.size() == 3 && parsed->assets[2].size == 1234, "assets and size");
        check(parsed->assets[2].sha256 == kDigest.substr(7), "digest taken as lowercase hex");
        check(parsed->is_app_release(), "v tag is an app release");
    }

    // GitHub's nulls: body, name, prerelease, digest, size, asset name.
    json nulls = standard_release();
    nulls["body"] = nullptr;
    nulls["name"] = nullptr;
    nulls["prerelease"] = nullptr;
    nulls["published_at"] = nullptr;
    nulls["assets"].push_back(asset("x", "https://github.com/x", nullptr, nullptr));
    nulls["assets"].push_back({{"name", nullptr}, {"browser_download_url", nullptr}});
    nulls["assets"].push_back(nullptr);
    const auto with_nulls = Releases::parse_release(nulls);
    check(with_nulls.has_value(), "null fields do not fail parsing");
    if (with_nulls) {
        check(with_nulls->body.empty() && with_nulls->name.empty() && !with_nulls->prerelease, "null fields become empty/false");
        check(with_nulls->assets.size() == 5, "null asset entries are skipped, null asset fields kept empty");
    }

    check(!Releases::parse_release(json::array()), "non-object is an error");
    check(!Releases::parse_release(json{{"tag_name", nullptr}}), "null tag is an error");
    check(!Releases::parse_release(json{{"assets", json::array()}}), "missing tag is an error");
    const auto no_assets = Releases::parse_release(json{{"tag_name", "v1.0.0"}, {"assets", nullptr}});
    check(no_assets && no_assets->assets.empty(), "null assets parse as none");

    check(Releases::digest_hex("sha256:ABCdef") == "abcdef", "digest is lowercased");
    check(Releases::digest_hex("sha1:abc").empty() && Releases::digest_hex("").empty(), "other digests are ignored");

    // A list survives a malformed entry.
    const auto list = Releases::parse_release_list(
        json::array({standard_release(), nullptr, json{{"name", "no tag"}}, release("scene-previews", json::array())}));
    check(list && list->size() == 2, "malformed list entries are skipped");
    check(!Releases::parse_release_list(json::object()), "a non-array list is an error");
}

void test_assets()
{
    const auto parsed = Releases::parse_release(standard_release());
    const auto pi = Releases::select_asset(*parsed, Releases::is_matrix_package, "led-matrix arm64");
    check(pi && pi->download_url == kDeb, "the Pi package is the arm64 .deb, not the tar.gz or the desktop package");

    const auto desktop =
        Releases::select_asset(*parsed, Releases::asset_named("led-matrix-desktop-1.20.0-Linux.tar.gz"), "desktop package");
    check(desktop && desktop->sha256 == "aa", "the desktop package is matched by exact name");
    check(!Releases::select_asset(*parsed, Releases::asset_named("led-matrix-desktop-1.20.0-win64.zip"), "desktop package"),
          "no asset for another platform");
    const auto missing = Releases::select_asset(*parsed, Releases::asset_named("nope"), "desktop package");
    check(!missing && missing.error() == "Could not find desktop package asset in release", "missing asset message");

    check(Releases::is_allowed_download_url(kDeb), "github.com is allowed");
    for (const char* bad :
         {"", "https://github.com/", "http://github.com/a/b", "https://github.com.evil.example/a", "https://github.com@evil.example/a",
          "https://evil.example/https://github.com/a", "https://api.github.com/a", "ftp://github.com/a", "https://github.com/a b"})
        check(!Releases::is_allowed_download_url(bad), std::string("download host refused: ") + bad);

    json foreign = standard_release();
    foreign["assets"][2]["browser_download_url"] = "https://example.com/led-matrix-1.20.0_arm64.deb";
    const auto refused = Releases::select_asset(*Releases::parse_release(foreign), Releases::is_matrix_package, "led-matrix arm64");
    check(!refused && refused.error().starts_with("Refusing update asset outside https://github.com/"), "foreign host is refused");
}

void test_verification()
{
    const auto dir = std::filesystem::temp_directory_path() / "releases_smoke";
    std::filesystem::create_directories(dir);
    const auto file = dir / "package.deb";
    std::ofstream(file, std::ios::binary) << "abc";
    const std::string abc = kDigest.substr(7);

    check(Releases::sha256_file(file) == abc, "sha256 of the file");
    check(Releases::verify_sha256(file, abc).has_value(), "matching digest verifies");
    std::string upper = abc;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    check(Releases::verify_sha256(file, upper).has_value(), "digest case does not matter");
    const auto mismatch = Releases::verify_sha256(file, std::string(64, '0'));
    check(!mismatch && mismatch.error().find("SHA-256 mismatch") != std::string::npos, "wrong digest is a mismatch");
    check(!Releases::verify_sha256(dir / "missing.deb", abc), "missing file does not verify");

    // The one policy: no valid digest, no install.
    for (const char* bad : {"", "887d1a2c", "zz", "not-hex-not-hex-not-hex-not-hex-not-hex-not-hex-not-hex-not-hex!"}) {
        check(!Releases::require_digest(bad, "pkg.deb"), std::string("digest refused: '") + bad + "'");
        check(!Releases::verify_sha256(file, bad), std::string("verify refuses without a valid digest: '") + bad + "'");
    }
    check(Releases::require_digest(abc, "pkg.deb").has_value(), "a valid digest is accepted");
    check(Releases::require_digest("", "pkg.deb").error().find("no valid sha256 digest") != std::string::npos, "refusal says why");
    std::filesystem::remove_all(dir);
}

void test_fetch()
{
    Recorder net;
    net.response = {200, standard_release().dump(), ""};
    Releases::FetchOptions options = Releases::make_fetch_options("led-matrix-test", Common::Version(1, 2, 3), 7000);
    options.transport = net.transport();

    const auto latest = Releases::fetch_latest(options);
    check(latest && latest->tag == "v1.20.0", "fetch_latest parses the body");
    check(net.urls.back() == "https://api.github.com/repos/sshcrack/led-matrix/releases/latest", "latest url");
    check(net.options.back().user_agent == "led-matrix-test/1.2.3 (https://github.com/sshcrack/led-matrix)", "User-Agent is sent");
    check(net.options.back().timeout_ms == 7000 && net.options.back().connect_timeout_ms == 5000, "timeouts are sent");

    check(Releases::fetch_version(Common::Version(1, 19, 4), options).has_value(), "fetch_version parses");
    check(net.urls.back() == "https://api.github.com/repos/sshcrack/led-matrix/releases/tags/v1.19.4", "tag url");

    net.response = {200, json::array({standard_release(), nullptr}).dump(), ""};
    const auto list = Releases::fetch_list(5, options);
    check(list && list->size() == 1, "fetch_list skips the broken entry");
    check(net.urls.back() == "https://api.github.com/repos/sshcrack/led-matrix/releases?per_page=5", "list url");
    Releases::fetch_list(500, options);
    check(net.urls.back().ends_with("per_page=100"), "per_page is clamped");
    Releases::fetch_list(0, options);
    check(net.urls.back().ends_with("per_page=1"), "per_page is at least 1");

    net.response = {404, "{}", ""};
    const auto missing = Releases::fetch_version(Common::Version(9, 9, 9), options);
    check(!missing && missing.error() == "No release v9.9.9 on GitHub", "404 for a tag");
    check(Releases::fetch_latest(options).error() == "No release on GitHub", "404 for latest");
    net.response = {503, "", ""};
    check(Releases::fetch_list(5, options).error() == "GitHub returned HTTP 503", "HTTP failure");
    net.response = {0, "", "Could not resolve host"};
    check(Releases::fetch_latest(options).error() == "GitHub request failed: Could not resolve host", "transport failure");
    net.response = {200, "<html>", ""};
    check(!Releases::fetch_latest(options) && !Releases::fetch_list(5, options), "unparsable body is an error, not an exception");

    options.transport = [](const std::string&, const Releases::FetchOptions&) -> Releases::HttpResponse {
        throw std::runtime_error("boom");
    };
    check(Releases::fetch_latest(options).error() == "GitHub request failed: boom", "a throwing transport is an error");
}

void test_route_json()
{
    // The route used to throw on a null asset name and lose the whole list.
    json broken = standard_release();
    broken["name"] = nullptr;
    broken["body"] = nullptr;
    broken["assets"].push_back({{"name", nullptr}});
    const auto releases = Releases::parse_release_list(json::array({
        broken,
        release("scene-previews", json::array({asset("led-matrix-1_arm64.deb", kDeb, kDigest)})),
        release("v1.0.0", json::array()),
        json{{"tag_name", "v0.9.0"}, {"assets", nullptr}, {"prerelease", true}},
    }));
    const json out = Releases::release_list_json(*releases, Releases::is_matrix_package);
    check(out.is_array() && out.size() == 3, "scene-previews is not an app version");
    check(out[0]["version"] == "v1.20.0" && out[0]["name"] == "" && out[0]["body"] == "", "null name/body become empty strings");
    check(out[0]["download_url"] == kDeb && out[0]["download_size"] == 1234, "download_url from the arm64 .deb only");
    check(out[0]["is_prerelease"] == false && out[0]["is_draft"] == false, "flags");
    check(!out[1].contains("download_url"), "a release without a package has no download_url");
    check(out[2]["is_prerelease"] == true && !out[2].contains("download_url"), "null assets");

    // Through the injected transport, the way the route runs it.
    Recorder net;
    net.response = {200, json::array({broken}).dump(), ""};
    Releases::FetchOptions options = Releases::make_fetch_options("led-matrix", Common::Version(1, 0, 0), 15000);
    options.transport = net.transport();
    const auto fetched = Releases::fetch_list(5, options);
    check(fetched && Releases::release_list_json(*fetched, Releases::is_matrix_package).size() == 1, "route path end to end");
    check(!net.options.back().user_agent.empty() && net.options.back().timeout_ms == 15000, "the route sends a User-Agent and a timeout");
}
}  // namespace

int main()
{
    test_status();
    test_parse();
    test_assets();
    test_verification();
    test_fetch();
    test_route_json();
    if (failures == 0)
        std::cout << "releases_smoke: ok\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

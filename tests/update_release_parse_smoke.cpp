// Release-JSON parsing for the Pi auto-updater. GitHub returns `"body": null` for a release
// without notes (v1.17.0 / v1.18.0 shipped like that), which used to make every update check fail.
#include "shared/matrix/update/UpdateManager.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool ok, const std::string &what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

nlohmann::json release(nlohmann::json body, nlohmann::json digest) {
    return {
        {"tag_name", "v1.18.0"},
        {"body", std::move(body)},
        {"prerelease", nullptr},
        {"assets", nlohmann::json::array({
            {{"name", "led-matrix-desktop-1.18.0-Linux.deb"},
             {"browser_download_url", "https://github.com/sshcrack/led-matrix/releases/download/v1.18.0/led-matrix-desktop-1.18.0-Linux.deb"},
             {"digest", "sha256:aaaa"}},
            {{"name", "led-matrix-1.18.0_arm64.deb"},
             {"browser_download_url", "https://github.com/sshcrack/led-matrix/releases/download/v1.18.0/led-matrix-1.18.0_arm64.deb"},
             {"digest", std::move(digest)}},
        })},
    };
}
}

int main() {
    using Update::UpdateManager;

    // Null body, null prerelease and a real digest: must parse.
    {
        const auto info = UpdateManager::parse_release(release(nullptr, "sha256:887d1a2c"));
        check(info.has_value(), std::string("null body parses: ") + (info ? "" : info.error()));
        if (info) {
            check(info->body.empty(), "null body becomes empty string");
            check(!info->is_prerelease, "null prerelease becomes false");
            check(info->sha256 == "887d1a2c", "sha256 taken from arm64 deb digest");
            check(info->download_url.ends_with("led-matrix-1.18.0_arm64.deb"), "picks the arm64 deb, not the desktop deb");
            check(info->version == Common::Version::fromString("1.18.0"), "version from tag without v prefix");
        }
    }

    // Notes present, digest null/missing: notes kept, sha256 empty.
    {
        const auto info = UpdateManager::parse_release(release("# What's changed", nullptr));
        check(info.has_value(), "null digest parses");
        if (info) {
            check(info->body == "# What's changed", "body kept");
            check(info->sha256.empty(), "null digest gives empty sha256");
        }
    }

    // Explicit version overrides the tag.
    {
        const auto info = UpdateManager::parse_release(release("", "sha256:ab"), Common::Version::fromString("1.17.0"));
        check(info && info->version == Common::Version::fromString("1.17.0"), "explicit version wins");
    }

    // Failures are reported, not thrown.
    {
        auto no_asset = release("", "sha256:ab");
        no_asset["assets"] = nlohmann::json::array();
        check(!UpdateManager::parse_release(no_asset).has_value(), "missing arm64 asset is an error");

        auto foreign = release("", "sha256:ab");
        foreign["assets"][1]["browser_download_url"] = "https://example.com/led-matrix-1.18.0_arm64.deb";
        check(!UpdateManager::parse_release(foreign).has_value(), "non-github download url is rejected");

        auto no_tag = release("", "sha256:ab");
        no_tag["tag_name"] = nullptr;
        check(!UpdateManager::parse_release(no_tag).has_value(), "null tag_name is an error");

        check(!UpdateManager::parse_release(nlohmann::json::array()).has_value(), "non-object is an error");
    }

    if (failures == 0) std::cout << "update_release_parse_smoke: ok\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

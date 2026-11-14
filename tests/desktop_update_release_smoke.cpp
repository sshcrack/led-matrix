// Desktop update paths on fixtures, no network: release parsing (UpdateChecker), the matrix's
// GET /api/update/status (by name, or by the int of an older matrix) and the refusal to install a package
// without a valid SHA-256 digest (SelfUpdate).
#include <cstdlib>
#include <iostream>
#include <string>

#include "shared/desktop/SelfUpdate.h"
#include "shared/desktop/UpdateChecker.h"

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

json release(json digest, const std::string& url_prefix = "https://github.com/sshcrack/led-matrix/releases/download/v1.20.0/")
{
    const std::string name = SelfUpdate::package_asset_name("1.20.0");
    return {
        {"tag_name", "v1.20.0"},
        {"name", nullptr},
        {"body", nullptr},
        {"prerelease", nullptr},
        {"html_url", "https://github.com/sshcrack/led-matrix/releases/tag/v1.20.0"},
        {"assets", json::array({
                       {{"name", nullptr}, {"browser_download_url", nullptr}},  // used to throw
                       {{"name", "led-matrix-1.20.0_arm64.deb"},
                        {"browser_download_url", url_prefix + "led-matrix-1.20.0_arm64.deb"},
                        {"digest", "sha256:bb"}},
                       {{"name", name}, {"browser_download_url", url_prefix + name}, {"digest", std::move(digest)}},
                   })},
    };
}

const std::string kSha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
}  // namespace

int main()
{
    using UpdateChecker::UpdateChecker;

    // Release parsing.
    {
        const auto info = UpdateChecker::parseReleaseInfo(release("sha256:" + kSha));
        check(info.version == Common::Version::fromString("1.20.0") && info.tagName == "v1.20.0", "tag and version");
        check(info.downloadUrl.ends_with(SelfUpdate::package_asset_name("1.20.0")), "this platform's package is chosen");
        check(info.sha256 == kSha, "sha256 from the asset digest");
        check(info.name.empty() && !info.isPrerelease, "null name and prerelease");

        const auto nodigest = UpdateChecker::parseReleaseInfo(release(nullptr));
        check(!nodigest.downloadUrl.empty() && nodigest.sha256.empty(), "a release without a digest still parses");

        const auto foreign = UpdateChecker::parseReleaseInfo(release("sha256:" + kSha, "https://example.com/"));
        check(foreign.downloadUrl.empty(), "a download host outside github.com is not offered");

        const auto none = UpdateChecker::parseReleaseInfo(json{{"tag_name", "v1.20.0"}, {"assets", nullptr}});
        check(none.downloadUrl.empty() && !none.version.is_invalid(), "no assets");
        check(UpdateChecker::parseReleaseInfo(json::array()).version.is_invalid(), "non-object release");
    }

    // The matrix's status, from a current and from an older matrix.
    {
        json current = {{"current_version", "1.20.0"}, {"auto_update_enabled", true}, {"status", 2}, {"status_name", "downloading"}};
        auto state = UpdateChecker::parseMatrixState(current);
        check(state && state->busy && state->autoUpdateEnabled && state->version == Common::Version::fromString("1.20.0"), "named status");
        check(state && state->status == Releases::UpdateStatus::DOWNLOADING, "status enum");

        json legacy = {{"current_version", "1.19.0"}, {"auto_update_enabled", false}, {"status", 3}};
        state = UpdateChecker::parseMatrixState(legacy);
        check(state && state->busy && !state->autoUpdateEnabled, "an older matrix sends only the int");

        for (const int code : {0, 4, 5, 6}) {
            state = UpdateChecker::parseMatrixState(json{{"current_version", "1.19.0"}, {"status", code}});
            check(state && !state->busy, "int " + std::to_string(code) + " is not busy");
        }
        state = UpdateChecker::parseMatrixState(json{{"current_version", "1.21.0"}, {"status_name", "checking"}});
        check(state && state->busy, "a newer matrix may send only the name");
        state = UpdateChecker::parseMatrixState(json{{"current_version", "1.21.0"}, {"status_name", "later-state"}, {"status", 4}});
        check(state && !state->busy && state->status == Releases::UpdateStatus::ERROR, "an unknown name falls back to the int");
        state = UpdateChecker::parseMatrixState(json{{"current_version", "1.21.0"}, {"status_name", "later-state"}});
        check(state && !state->busy && !state->status, "an unknown status alone is not busy");

        check(!UpdateChecker::parseMatrixState(json{{"auto_update_enabled", true}}), "no version is an error");
        check(!UpdateChecker::parseMatrixState(json{{"current_version", nullptr}}), "null version is an error");
        check(!UpdateChecker::parseMatrixState(json::array()), "non-object status");
    }

    // Never install an unverified package: refused before any download or install directory is touched.
    {
        const std::string url =
            "https://github.com/sshcrack/led-matrix/releases/download/v1.20.0/" + SelfUpdate::package_asset_name("1.20.0");
        const auto noDigest = SelfUpdate::download_and_install(url, "", "1.20.0", {});
        check(!noDigest && noDigest.error().find("no valid sha256 digest") != std::string::npos, "no digest is refused");
        check(!SelfUpdate::download_and_install(url, "887d1a2c", "1.20.0", {}), "a short digest is refused");
        const auto foreign = SelfUpdate::download_and_install("https://example.com/p.tar.gz", kSha, "1.20.0", {});
        check(!foreign && foreign.error().find("Refusing to download") != std::string::npos, "a foreign host is refused");
    }

    if (failures == 0)
        std::cout << "desktop_update_release_smoke: ok\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

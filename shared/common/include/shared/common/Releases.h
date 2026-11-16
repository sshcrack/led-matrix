#pragma once

// One implementation of GitHub release handling for everything that talks to
// https://github.com/sshcrack/led-matrix/releases: the Pi's UpdateManager, the
// desktop's UpdateChecker/SelfUpdate and the GET /api/update/releases route.
//
// It owns
//   * fetching (latest, by version tag, list) with a timeout and a User-Agent,
//   * null-safe parsing (GitHub sends null for empty body/name/digest),
//   * asset choice (which file is "the package" for the Pi / this desktop),
//   * the allowed download hosts,
//   * the one SHA-256 check and its policy: a package without a valid digest is
//     never installed. Parsing a release without a digest still succeeds; only
//     installing it is refused (`require_digest`),
//   * the update status shared between the Pi and the desktop, with its JSON names.
//
// Nothing here installs anything. The callers stay thin: they pick the asset,
// download it, call `verify_sha256`, then do their own platform-specific install.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "shared/common/Version.h"
#include "shared/common/macro.h"

// Windows.h defines ERROR as 0, which collides with UpdateStatus::ERROR
#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace Releases {
// ---- update status shared by the Pi and the desktop ---------------------------------------

/// Progress of an update on the Pi. The numeric values are the legacy wire format (0..6) and must
/// not change; new states are appended.
enum class UpdateStatus {
    IDLE = 0,
    CHECKING = 1,  // a manual install is resolving its release (set before the install request is answered)
    DOWNLOADING = 2,
    INSTALLING = 3,
    ERROR = 4,
    SUCCESS = 5,
    DISABLED = 6  // Auto-update is disabled
};

/// JSON name: "idle", "checking", "downloading", "installing", "error", "success", "disabled".
SHARED_COMMON_API const char* status_name(UpdateStatus status);
SHARED_COMMON_API std::optional<UpdateStatus> status_from_name(std::string_view name);
/// Legacy numeric value; nullopt when out of range.
SHARED_COMMON_API std::optional<UpdateStatus> status_from_code(int code);
/// Checking, downloading or installing: an install is in flight.
SHARED_COMMON_API bool is_busy(UpdateStatus status);

/// Writes `"status"` (the legacy int, kept for older desktops and the web UI) and `"status_name"`.
SHARED_COMMON_API void write_status(nlohmann::json& object, UpdateStatus status);
/// Reads a status written by `write_status`, or by an older Pi that sent only the int. The name wins
/// when both are present and the name is known; an unknown name falls back to the int.
SHARED_COMMON_API std::optional<UpdateStatus> read_status(const nlohmann::json& object);

// ---- releases and assets ------------------------------------------------------------------

struct Asset {
    std::string name;
    std::string download_url;  ///< browser_download_url
    /// Lowercase hex from GitHub's "sha256:<hex>" digest; empty when GitHub sent none. Not validated
    /// here: parsing a release without a (valid) digest succeeds, installing it does not (`require_digest`).
    std::string sha256;
    std::int64_t size = 0;
};

struct Release {
    std::string tag;  ///< tag_name, e.g. "v1.22.2"
    std::string name;
    std::string body;
    std::string html_url;
    std::string published_at;
    Common::Version version;  ///< invalid (0.0.0) when the tag is not a version
    bool prerelease = false;
    bool draft = false;
    std::vector<Asset> assets;

    /// The rolling "scene-previews" release is an asset store, not an app version.
    [[nodiscard]] bool is_app_release() const { return tag.starts_with('v'); }
};

/// Null-safe: a null or missing optional field (body, name, prerelease, digest, size, ...) is
/// treated like a missing key. Fails only when `release` is not an object or has no tag_name.
SHARED_COMMON_API std::expected<Release, std::string> parse_release(const nlohmann::json& release);
/// A JSON array of releases. One malformed entry is skipped instead of failing the whole list.
SHARED_COMMON_API std::expected<std::vector<Release>, std::string> parse_release_list(const nlohmann::json& releases);

/// "sha256:<hex>" (GitHub's asset digest) to lowercase hex; empty for anything else.
SHARED_COMMON_API std::string digest_hex(std::string_view digest);

// ---- asset choice -------------------------------------------------------------------------

using AssetMatcher = std::function<bool(const Asset&)>;

/// The Pi package: led-matrix-<version>_arm64.deb (not the desktop package, not a tar.gz).
SHARED_COMMON_API bool is_matrix_package(const Asset& asset);
/// Matches one exact asset name, e.g. the desktop package of this platform.
SHARED_COMMON_API AssetMatcher asset_named(std::string name);

SHARED_COMMON_API const Asset* find_asset(const Release& release, const AssetMatcher& matcher);

/// Only `https://github.com/` serves release downloads (redirects to the CDN are followed by the
/// HTTP client afterwards). Everything else, including http:// and lookalike hosts, is refused.
SHARED_COMMON_API bool is_allowed_download_url(std::string_view url);

/// The first asset accepted by `matcher`, if its download URL is on an allowed host.
/// `what` names it for the error message ("led-matrix arm64", "desktop package").
SHARED_COMMON_API std::expected<Asset, std::string> select_asset(const Release& release, const AssetMatcher& matcher,
                                                                 std::string_view what);

// ---- verification -------------------------------------------------------------------------

/// Exactly 64 hex digits.
SHARED_COMMON_API bool is_valid_sha256(std::string_view hex);
/// The install policy: an asset without a valid SHA-256 digest is refused. Call before downloading
/// (saves the download) and rely on `verify_sha256` after it.
SHARED_COMMON_API std::expected<void, std::string> require_digest(std::string_view sha256, std::string_view asset_name);
SHARED_COMMON_API std::expected<std::string, std::string> sha256_file(const std::filesystem::path& file);
/// Requires a valid expected digest, then compares it with the file's. The only SHA-256 check.
SHARED_COMMON_API std::expected<void, std::string> verify_sha256(const std::filesystem::path& file, std::string_view expected_hex);

// ---- fetching -----------------------------------------------------------------------------

struct HttpResponse {
    long status = 0;  ///< 0 when no response arrived
    std::string body;
    std::string error;  ///< transport error, when status is 0
};

struct FetchOptions;
using HttpGet = std::function<HttpResponse(const std::string& url, const FetchOptions& options)>;

struct FetchOptions {
    std::string user_agent;
    std::string repo = "sshcrack/led-matrix";  ///< owner/name
    int connect_timeout_ms = 5000;
    int timeout_ms = 15000;
    /// Replaces the HTTP client, so tests feed fixture JSON without a network.
    HttpGet transport;
};

/// Options for a caller named `product` ("led-matrix", "led-matrix-desktop"): sets the User-Agent
/// ("<product>/<version> (https://github.com/sshcrack/led-matrix)") and the total timeout.
SHARED_COMMON_API FetchOptions make_fetch_options(std::string_view product, const Common::Version& version, int timeout_ms);
SHARED_COMMON_API std::string releases_url(const FetchOptions& options);

SHARED_COMMON_API std::expected<Release, std::string> fetch_latest(const FetchOptions& options);
/// The release tagged "v<version>".
SHARED_COMMON_API std::expected<Release, std::string> fetch_version(const Common::Version& version, const FetchOptions& options);
/// The most recent releases (GitHub returns them newest first), `per_page` clamped to 1..100.
SHARED_COMMON_API std::expected<std::vector<Release>, std::string> fetch_list(int per_page, const FetchOptions& options);

// ---- JSON shapes --------------------------------------------------------------------------

/// One entry of GET /api/update/releases. `download_url` / `download_size` are present only when
/// `matcher` finds an asset on an allowed host.
SHARED_COMMON_API nlohmann::json release_summary_json(const Release& release, const AssetMatcher& matcher);
/// The whole GET /api/update/releases body: app releases only (no "scene-previews" asset store).
SHARED_COMMON_API nlohmann::json release_list_json(const std::vector<Release>& releases, const AssetMatcher& matcher);
}  // namespace Releases

#include "shared/common/Releases.h"

#include <cpr/cpr.h>
#include <picosha2.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace Releases {
namespace {
constexpr std::string_view kDownloadHost = "https://github.com/";

// GitHub sends null for empty optional fields (a release without notes, an asset without a
// digest); nlohmann's value() throws on those, so treat null like a missing key.
std::string string_or(const nlohmann::json& obj, const char* key)
{
    const auto it = obj.find(key);
    return it != obj.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

bool bool_or(const nlohmann::json& obj, const char* key, bool fallback)
{
    const auto it = obj.find(key);
    return it != obj.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

std::int64_t int_or(const nlohmann::json& obj, const char* key, std::int64_t fallback)
{
    const auto it = obj.find(key);
    return it != obj.end() && it->is_number_integer() ? it->get<std::int64_t>() : fallback;
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

HttpResponse cpr_get(const std::string& url, const FetchOptions& options)
{
    cpr::Header headers;
    if (!options.user_agent.empty())
        headers["User-Agent"] = options.user_agent;
    const cpr::Response response =
        cpr::Get(cpr::Url{url}, headers, cpr::ConnectTimeout{options.connect_timeout_ms}, cpr::Timeout{options.timeout_ms});
    return {response.status_code, response.text, response.error ? response.error.message : std::string{}};
}

std::expected<std::string, std::string> get_body(const std::string& url, const FetchOptions& options, const std::string& not_found)
{
    HttpResponse response;
    try {
        response = options.transport ? options.transport(url, options) : cpr_get(url, options);
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("GitHub request failed: ") + e.what());
    }
    if (response.status == 404 && !not_found.empty())
        return std::unexpected(not_found);
    if (response.status == 0)
        return std::unexpected("GitHub request failed" + (response.error.empty() ? "" : ": " + response.error));
    if (response.status != 200)
        return std::unexpected("GitHub returned HTTP " + std::to_string(response.status));
    return std::move(response.body);
}
}  // namespace

// ---- status -----------------------------------------------------------------------------------

const char* status_name(UpdateStatus status)
{
    switch (status) {
        case UpdateStatus::IDLE:
            return "idle";
        case UpdateStatus::CHECKING:
            return "checking";
        case UpdateStatus::DOWNLOADING:
            return "downloading";
        case UpdateStatus::INSTALLING:
            return "installing";
        case UpdateStatus::ERROR:
            return "error";
        case UpdateStatus::SUCCESS:
            return "success";
        case UpdateStatus::DISABLED:
            return "disabled";
    }
    return "idle";
}

std::optional<UpdateStatus> status_from_name(std::string_view name)
{
    for (int code = 0; code <= static_cast<int>(UpdateStatus::DISABLED); ++code)
        if (name == status_name(static_cast<UpdateStatus>(code)))
            return static_cast<UpdateStatus>(code);
    return std::nullopt;
}

std::optional<UpdateStatus> status_from_code(int code)
{
    if (code < static_cast<int>(UpdateStatus::IDLE) || code > static_cast<int>(UpdateStatus::DISABLED))
        return std::nullopt;
    return static_cast<UpdateStatus>(code);
}

bool is_busy(UpdateStatus status)
{
    return status == UpdateStatus::CHECKING || status == UpdateStatus::DOWNLOADING || status == UpdateStatus::INSTALLING;
}

void write_status(nlohmann::json& object, UpdateStatus status)
{
    object["status"] = static_cast<int>(status);
    object["status_name"] = status_name(status);
}

std::optional<UpdateStatus> read_status(const nlohmann::json& object)
{
    if (!object.is_object())
        return std::nullopt;
    if (const auto it = object.find("status_name"); it != object.end() && it->is_string())
        if (const auto named = status_from_name(it->get<std::string>()))
            return named;
    if (const auto it = object.find("status"); it != object.end() && it->is_number_integer())
        return status_from_code(it->get<int>());
    return std::nullopt;
}

// ---- parsing ----------------------------------------------------------------------------------

std::string digest_hex(std::string_view digest)
{
    constexpr std::string_view prefix = "sha256:";
    return digest.starts_with(prefix) ? lowered(digest.substr(prefix.size())) : std::string{};
}

std::expected<Release, std::string> parse_release(const nlohmann::json& json)
{
    if (!json.is_object())
        return std::unexpected("Release response is not a JSON object");

    Release release;
    release.tag = string_or(json, "tag_name");
    if (release.tag.empty())
        return std::unexpected("Release has no tag_name");
    release.name = string_or(json, "name");
    release.body = string_or(json, "body");
    release.html_url = string_or(json, "html_url");
    release.published_at = string_or(json, "published_at");
    release.prerelease = bool_or(json, "prerelease", false);
    release.draft = bool_or(json, "draft", false);
    release.version = Common::Version::fromString(release.tag);  // accepts a "v" prefix

    const auto assets = json.find("assets");
    if (assets != json.end() && assets->is_array()) {
        for (const auto& entry : *assets) {
            if (!entry.is_object())
                continue;
            Asset asset;
            asset.name = string_or(entry, "name");
            asset.download_url = string_or(entry, "browser_download_url");
            asset.sha256 = digest_hex(string_or(entry, "digest"));
            asset.size = int_or(entry, "size", 0);
            release.assets.push_back(std::move(asset));
        }
    }
    return release;
}

std::expected<std::vector<Release>, std::string> parse_release_list(const nlohmann::json& json)
{
    if (!json.is_array())
        return std::unexpected("Release list response is not a JSON array");
    std::vector<Release> releases;
    for (const auto& entry : json)
        if (auto release = parse_release(entry))
            releases.push_back(std::move(*release));
    return releases;
}

// ---- assets -----------------------------------------------------------------------------------

bool is_matrix_package(const Asset& asset)
{
    return asset.name.find("led-matrix") != std::string::npos && asset.name.find("arm64") != std::string::npos &&
           asset.name.ends_with(".deb");
}

AssetMatcher asset_named(std::string name)
{
    return [name = std::move(name)](const Asset& asset) { return asset.name == name; };
}

const Asset* find_asset(const Release& release, const AssetMatcher& matcher)
{
    const auto it = std::ranges::find_if(release.assets, [&](const Asset& asset) { return matcher(asset); });
    return it == release.assets.end() ? nullptr : &*it;
}

bool is_allowed_download_url(std::string_view url)
{
    // The trailing slash ends the host, so "https://github.com.evil.example/" and
    // "https://github.com@evil.example/" do not match.
    return url.starts_with(kDownloadHost) && url.size() > kDownloadHost.size() &&
           std::ranges::none_of(url, [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); });
}

std::expected<Asset, std::string> select_asset(const Release& release, const AssetMatcher& matcher, std::string_view what)
{
    const Asset* asset = find_asset(release, matcher);
    if (!asset)
        return std::unexpected("Could not find " + std::string(what) + " asset in release");
    if (!is_allowed_download_url(asset->download_url))
        return std::unexpected("Refusing update asset outside " + std::string(kDownloadHost) + ": " + asset->download_url);
    return *asset;
}

// ---- verification -----------------------------------------------------------------------------

bool is_valid_sha256(std::string_view hex)
{
    return hex.size() == picosha2::k_digest_size * 2 && std::ranges::all_of(hex, [](unsigned char c) { return std::isxdigit(c) != 0; });
}

std::expected<void, std::string> require_digest(std::string_view sha256, std::string_view asset_name)
{
    if (is_valid_sha256(sha256))
        return {};
    return std::unexpected("The release has no valid sha256 digest for " + std::string(asset_name) +
                           ", so it cannot be verified and is not installed. Install it manually from the "
                           "releases page.");
}

std::expected<std::string, std::string> sha256_file(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::unexpected("Could not open " + file.string() + " for verification");
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>(), hash.begin(), hash.end());
    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

std::expected<void, std::string> verify_sha256(const std::filesystem::path& file, std::string_view expected_hex)
{
    if (!is_valid_sha256(expected_hex))
        return require_digest(expected_hex, file.filename().string());
    const auto actual = sha256_file(file);
    if (!actual)
        return std::unexpected(actual.error());
    const std::string expected = lowered(expected_hex);
    if (*actual != expected)
        return std::unexpected("SHA-256 mismatch for " + file.filename().string() + " (expected " + expected + ", got " + *actual + ")");
    return {};
}

// ---- fetching ---------------------------------------------------------------------------------

FetchOptions make_fetch_options(std::string_view product, const Common::Version& version, int timeout_ms)
{
    FetchOptions options;
    options.user_agent = std::string(product) + "/" + version.toString() + " (https://github.com/sshcrack/led-matrix)";
    options.timeout_ms = timeout_ms;
    return options;
}

std::string releases_url(const FetchOptions& options)
{
    return "https://api.github.com/repos/" + options.repo + "/releases";
}

namespace {
std::expected<Release, std::string> fetch_one(const std::string& url, const FetchOptions& options, const std::string& not_found)
{
    const auto body = get_body(url, options, not_found);
    if (!body)
        return std::unexpected(body.error());
    try {
        return parse_release(nlohmann::json::parse(*body));
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Could not parse the GitHub release: ") + e.what());
    }
}
}  // namespace

std::expected<Release, std::string> fetch_latest(const FetchOptions& options)
{
    return fetch_one(releases_url(options) + "/latest", options, "No release on GitHub");
}

std::expected<Release, std::string> fetch_version(const Common::Version& version, const FetchOptions& options)
{
    return fetch_one(releases_url(options) + "/tags/v" + version.toString(), options, "No release v" + version.toString() + " on GitHub");
}

std::expected<std::vector<Release>, std::string> fetch_list(int per_page, const FetchOptions& options)
{
    per_page = std::clamp(per_page, 1, 100);
    const auto body = get_body(releases_url(options) + "?per_page=" + std::to_string(per_page), options, "");
    if (!body)
        return std::unexpected(body.error());
    try {
        return parse_release_list(nlohmann::json::parse(*body));
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Could not parse the GitHub releases: ") + e.what());
    }
}

// ---- JSON shapes ------------------------------------------------------------------------------

nlohmann::json release_summary_json(const Release& release, const AssetMatcher& matcher)
{
    nlohmann::json summary;
    summary["version"] = release.tag;
    summary["name"] = release.name;
    summary["body"] = release.body;
    summary["published_at"] = release.published_at;
    summary["is_prerelease"] = release.prerelease;
    summary["is_draft"] = release.draft;
    if (const auto asset = select_asset(release, matcher, "package")) {
        summary["download_url"] = asset->download_url;
        summary["download_size"] = asset->size;
    }
    return summary;
}

nlohmann::json release_list_json(const std::vector<Release>& releases, const AssetMatcher& matcher)
{
    nlohmann::json list = nlohmann::json::array();
    for (const auto& release : releases)
        if (release.is_app_release())
            list.push_back(release_summary_json(release, matcher));
    return list;
}
}  // namespace Releases

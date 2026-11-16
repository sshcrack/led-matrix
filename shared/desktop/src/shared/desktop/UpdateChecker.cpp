#include "shared/desktop/UpdateChecker.h"
#include "shared/desktop/SelfUpdate.h"
#include "shared/desktop/config.h"
#include "shared/desktop/utils.h"
#include "led_matrix_version.h" // generated
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

namespace UpdateChecker
{
namespace {
std::string userAgent()
{
    return "led-matrix-desktop/" + UpdateChecker::getCurrentVersion().toString();
}

        std::string matrixUrl(const MatrixEndpoint &matrix, const std::string &path)
        {
            return "http://" + matrix.host + ":" + std::to_string(matrix.port) + path;
        }

        cpr::Header matrixHeaders(const MatrixEndpoint &matrix)
        {
            cpr::Header headers{{"User-Agent", userAgent()}};
            if (!matrix.authorization.empty())
                headers["Authorization"] = matrix.authorization;
            return headers;
        }
        }  // namespace

    MatrixEndpoint MatrixEndpoint::fromConfig()
    {
        const auto &general = Config::ConfigManager::instance()->getGeneralConfig();
        return {general.getHostnameCopy(), general.getPort(), Config::matrixAuthorizationHeader()};
    }

    // ---- preferences ----------------------------------------------------

    void UpdatePreferences::save()
    {
        try
        {
            nlohmann::json j;
            j["skippedVersion"] = skippedVersion;
            j["lastRemindTime"] = lastRemindTime;
            j["updateNotificationsEnabled"] = updateNotificationsEnabled;
            j["remindIntervalDays"] = remindIntervalDays;
            j["autoUpdate"] = autoUpdate;
            j["followMatrix"] = followMatrix;
            std::ofstream(get_data_dir() / "update_preferences.json") << j.dump(4);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Failed to save update preferences: {}", e.what());
        }
    }

    void UpdatePreferences::load()
    {
        try
        {
            const auto path = get_data_dir() / "update_preferences.json";
            if (!std::filesystem::exists(path))
                return;
            std::ifstream file(path);
            nlohmann::json j;
            file >> j;
            skippedVersion = j.value("skippedVersion", "");
            lastRemindTime = j.value("lastRemindTime", int64_t(0));
            updateNotificationsEnabled = j.value("updateNotificationsEnabled", true);
            remindIntervalDays = j.value("remindIntervalDays", 7);
            autoUpdate = j.value("autoUpdate", true);
            followMatrix = j.value("followMatrix", true);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Failed to load update preferences: {}", e.what());
            *this = UpdatePreferences{};
        }
    }

    bool UpdatePreferences::shouldRemindForVersion(const std::string &version) const
    {
        if (!updateNotificationsEnabled || shouldSkipVersion(version))
            return false;
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
        return (now - lastRemindTime) / (24 * 60 * 60) >= remindIntervalDays;
    }

    bool UpdatePreferences::shouldSkipVersion(const std::string &version) const
    {
        return !skippedVersion.empty() && skippedVersion == version;
    }

    // ---- checker ----------------------------------------------------------

    Common::Version UpdateChecker::getCurrentVersion()
    {
        return Common::Version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
    }

    std::expected<ReleaseInfo, std::string> UpdateChecker::fetchRelease(const std::optional<Common::Version> &version)
    {
        const auto options = Releases::make_fetch_options("led-matrix-desktop", getCurrentVersion(), 15000);
        const auto release = version ? Releases::fetch_version(*version, options) : Releases::fetch_latest(options);
        if (!release)
            return std::unexpected(release.error());
        ReleaseInfo info = toReleaseInfo(*release);
        if (info.version.is_invalid())
            return std::unexpected("Unexpected release tag '" + info.tagName + "'");
        return info;
    }

    ReleaseInfo UpdateChecker::toReleaseInfo(const Releases::Release& release)
    {
        ReleaseInfo info;
        info.tagName = release.tag;
        info.name = release.name;
        info.htmlUrl = release.html_url;
        info.isPrerelease = release.prerelease;
        info.version = release.version;
        if (info.version.is_invalid())
            return info;

        const auto asset = Releases::select_asset(release, Releases::asset_named(SelfUpdate::package_asset_name(info.version.toString())),
                                                  "desktop package");
        if (asset) {
            info.downloadUrl = asset->download_url;
            info.sha256 = asset->sha256;
        }
        else
            spdlog::debug("Release {}: {}", release.tag, asset.error());
        return info;
    }

    ReleaseInfo UpdateChecker::parseReleaseInfo(const nlohmann::json& releaseJson)
    {
        const auto release = Releases::parse_release(releaseJson);
        return release ? toReleaseInfo(*release) : ReleaseInfo{};
    }

    std::expected<MatrixUpdateState, std::string> UpdateChecker::parseMatrixState(const nlohmann::json& status)
    {
        if (!status.is_object())
            return std::unexpected("the matrix update status is not a JSON object");
        const auto version = status.find("current_version");
        MatrixUpdateState state;
        if (version != status.end() && version->is_string())
            state.version = Common::Version::fromString(version->get<std::string>());
        if (state.version.is_invalid())
            return std::unexpected("matrix reported no version");
        const auto autoUpdate = status.find("auto_update_enabled");
        state.autoUpdateEnabled = autoUpdate != status.end() && autoUpdate->is_boolean() && autoUpdate->get<bool>();
        state.status = Releases::read_status(status);  // by name, or the int of an older matrix
        state.busy = state.status && Releases::is_busy(*state.status);
        return state;
    }

    std::expected<MatrixUpdateState, MatrixFetchError> UpdateChecker::fetchMatrixState(const MatrixEndpoint& matrix)
    {
        const auto response = cpr::Get(cpr::Url{matrixUrl(matrix, "/api/update/status")}, matrixHeaders(matrix),
                                       cpr::Timeout{5000});
        if (response.status_code == 0)
            return std::unexpected(MatrixFetchError{MatrixFetchError::Kind::Unreachable, "matrix unreachable"});
        if (response.status_code != 200)
            return std::unexpected(
                MatrixFetchError{MatrixFetchError::Kind::Http, "matrix returned HTTP " + std::to_string(response.status_code)});
        try
        {
            auto state = parseMatrixState(nlohmann::json::parse(response.text));
            if (!state)
                return std::unexpected(MatrixFetchError{MatrixFetchError::Kind::Parse, state.error()});
            return *state;
        }
        catch (const std::exception &e)
        {
            return std::unexpected(
                MatrixFetchError{MatrixFetchError::Kind::Parse, std::string("could not parse the matrix update status: ") + e.what()});
        }
    }

    std::expected<void, std::string> UpdateChecker::requestMatrixUpdate(const MatrixEndpoint &matrix,
                                                                         const Common::Version &version)
    {
        const auto response = cpr::Post(cpr::Url{matrixUrl(matrix, "/api/update/install")},
                                        cpr::Parameters{{"version", version.toString()}}, matrixHeaders(matrix),
                                        cpr::Timeout{10000});
        if (response.status_code == 202 || response.status_code == 409) // started / already running
            return {};
        return std::unexpected("matrix returned HTTP " + std::to_string(response.status_code));
    }

} // namespace UpdateChecker

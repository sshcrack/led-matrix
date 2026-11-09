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
    namespace
    {
        constexpr const char *RELEASES_API = "https://api.github.com/repos/sshcrack/led-matrix/releases";

        std::string userAgent() { return "led-matrix-desktop/" + UpdateChecker::getCurrentVersion().toString(); }

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
    } // namespace

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
        const std::string url = version ? std::string(RELEASES_API) + "/tags/v" + version->toString()
                                        : std::string(RELEASES_API) + "/latest";
        const auto response = cpr::Get(cpr::Url{url}, cpr::Header{{"User-Agent", userAgent()}}, cpr::Timeout{15000});
        if (response.status_code == 404)
            return std::unexpected(version ? "No release v" + version->toString() + " on GitHub" : "No release on GitHub");
        if (response.status_code != 200)
            return std::unexpected("GitHub returned HTTP " + std::to_string(response.status_code) +
                                   (response.error ? " (" + response.error.message + ")" : ""));
        try
        {
            ReleaseInfo info = parseReleaseInfo(nlohmann::json::parse(response.text));
            if (info.version.is_invalid())
                return std::unexpected("Unexpected release tag '" + info.tagName + "'");
            return info;
        }
        catch (const std::exception &e)
        {
            return std::unexpected(std::string("Could not parse the GitHub release: ") + e.what());
        }
    }

    ReleaseInfo UpdateChecker::parseReleaseInfo(const nlohmann::json &releaseJson)
    {
        ReleaseInfo info;
        auto str = [&](const char *key) {
            return releaseJson.contains(key) && releaseJson[key].is_string() ? releaseJson[key].get<std::string>() : std::string{};
        };
        info.tagName = str("tag_name");
        info.name = str("name");
        info.htmlUrl = str("html_url");
        info.isPrerelease = releaseJson.value("prerelease", false);
        info.version = Common::Version::fromString(info.tagName);

        const std::string assetName = SelfUpdate::package_asset_name(info.version.toString());
        if (releaseJson.contains("assets") && releaseJson["assets"].is_array())
        {
            for (const auto &asset: releaseJson["assets"])
            {
                if (asset.value("name", "") != assetName)
                    continue;
                info.downloadUrl = asset.value("browser_download_url", "");
                if (asset.contains("digest") && asset["digest"].is_string())
                {
                    const std::string digest = asset["digest"].get<std::string>();
                    if (digest.rfind("sha256:", 0) == 0)
                        info.sha256 = digest.substr(7);
                }
            }
        }
        return info;
    }

    std::expected<MatrixUpdateState, std::string> UpdateChecker::fetchMatrixState(const MatrixEndpoint &matrix)
    {
        const auto response = cpr::Get(cpr::Url{matrixUrl(matrix, "/api/update/status")}, matrixHeaders(matrix),
                                       cpr::Timeout{5000});
        if (response.status_code != 200)
            return std::unexpected(response.status_code == 0 ? "matrix unreachable"
                                                             : "matrix returned HTTP " + std::to_string(response.status_code));
        try
        {
            const auto j = nlohmann::json::parse(response.text);
            MatrixUpdateState state;
            state.version = Common::Version::fromString(j.value("current_version", ""));
            state.autoUpdateEnabled = j.value("auto_update_enabled", false);
            const int status = j.value("status", 0); // Update::UpdateStatus: 1 checking, 2 downloading, 3 installing
            state.busy = status >= 1 && status <= 3;
            if (state.version.is_invalid())
                return std::unexpected("matrix reported no version");
            return state;
        }
        catch (const std::exception &e)
        {
            return std::unexpected(std::string("could not parse the matrix update status: ") + e.what());
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

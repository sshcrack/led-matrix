#include "shared/desktop/UpdateChecker.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <regex>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <hello_imgui/hello_imgui.h>
#include <imgui.h>
#include <shared/desktop/utils.h>

#ifdef _WIN32
#include <shellapi.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <cstdlib>
#endif

// Include HTTP client library
#include <cpr/cpr.h>

namespace UpdateChecker
{

    // UpdatePreferences implementation
    void UpdatePreferences::save()
    {
        try
        {
            nlohmann::json j;
            j["skippedVersion"] = skippedVersion;
            j["lastRemindTime"] = lastRemindTime;
            j["updateNotificationsEnabled"] = updateNotificationsEnabled;
            j["remindIntervalDays"] = remindIntervalDays;

            std::filesystem::path configPath = get_data_dir() / "update_preferences.json";
            std::ofstream file(configPath);
            file << j.dump(4);

            spdlog::debug("Saved update preferences to: {}", configPath.string());
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
            std::filesystem::path configPath = get_data_dir() / "update_preferences.json";
            if (!std::filesystem::exists(configPath))
            {
                spdlog::debug("Update preferences file doesn't exist, using defaults");
                return;
            }

            std::ifstream file(configPath);
            nlohmann::json j;
            file >> j;

            skippedVersion = j.value("skippedVersion", "");
            lastRemindTime = j.value("lastRemindTime", int64_t(0));
            updateNotificationsEnabled = j.value("updateNotificationsEnabled", true);
            remindIntervalDays = j.value("remindIntervalDays", 7);

            spdlog::debug("Loaded update preferences from: {}", configPath.string());
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Failed to load update preferences: {}", e.what());
            // Use defaults on failure
            skippedVersion = "";
            lastRemindTime = 0;
            updateNotificationsEnabled = true;
            remindIntervalDays = 7;
        }
    }

    bool UpdatePreferences::shouldRemindForVersion(const std::string &version) const
    {
        if (!updateNotificationsEnabled)
        {
            return false;
        }

        // Check if this version is marked as skipped
        if (shouldSkipVersion(version))
        {
            return false;
        }

        // Check if enough time has passed since last remind
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        auto daysSinceRemind = (nowTime - lastRemindTime) / (24 * 60 * 60);

        return daysSinceRemind >= remindIntervalDays;
    }

    bool UpdatePreferences::shouldSkipVersion(const std::string &version) const
    {
        return !skippedVersion.empty() && skippedVersion == version;
    }

    // Version implementation
    Version Version::fromString(const std::string &versionStr)
    {
        Version version;
        std::regex versionRegex(R"(v?(\d+)\.(\d+)\.(\d+))");
        std::smatch matches;

        if (std::regex_search(versionStr, matches, versionRegex))
        {
            version.major = std::stoi(matches[1].str());
            version.minor = std::stoi(matches[2].str());
            version.patch = std::stoi(matches[3].str());
        }

        return version;
    }

    bool Version::operator>(const Version &other) const
    {
        if (major != other.major)
            return major > other.major;
        if (minor != other.minor)
            return minor > other.minor;
        return patch > other.patch;
    }

    bool Version::operator==(const Version &other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool Version::operator<(const Version &other) const
    {
        return !(*this > other) && !(*this == other);
    }

    std::string Version::toString() const
    {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    // UpdateChecker implementation
    struct UpdateChecker::Impl
    {
        bool isCheckingForUpdates = false;
        bool showUpdateDialogFlag = false;
        ReleaseInfo pendingRelease;
        std::function<void(UpdateAction)> pendingCallback;
        UpdatePreferences preferences;

        Impl()
        {
            preferences.load();
        }

        ~Impl()
        {
            preferences.save();
        }
    };

    UpdateChecker::UpdateChecker() : pImpl(std::make_unique<Impl>()) {}

    UpdateChecker::~UpdateChecker() = default;

    Version UpdateChecker::getCurrentVersion()
    {
        // Use compile-time definitions from CMakeLists.txt
        return Version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
    }

    UpdatePreferences &UpdateChecker::getPreferences()
    {
        return pImpl->preferences;
    }

    void UpdateChecker::checkForUpdates(std::function<void(bool hasUpdate, const ReleaseInfo &release)> callback)
    {
        if (pImpl->isCheckingForUpdates)
        {
            spdlog::warn("Update check already in progress");
            return;
        }

        pImpl->isCheckingForUpdates = true;

        // Run HTTP request in background thread
        std::thread([this, callback]()
                    {
        try {
            spdlog::info("Checking for updates from GitHub...");
            
            auto response = cpr::Get(
                cpr::Url{"https://api.github.com/repos/sshcrack/led-matrix/releases/latest"},
                cpr::Header{{"User-Agent", "led-matrix-desktop/" + getCurrentVersion().toString()}}
            );
            
            if (response.status_code == 200) {
                auto releaseJson = nlohmann::json::parse(response.text);
                ReleaseInfo release = parseReleaseInfo(releaseJson);
                
                Version currentVersion = getCurrentVersion();
                bool hasUpdate = release.version > currentVersion;
                
                // Check preferences to see if we should notify about this update
                if (hasUpdate && !pImpl->preferences.shouldRemindForVersion(release.tagName)) {
                    spdlog::info("Update available but skipped due to user preferences: {}", release.tagName);
                    hasUpdate = false; // Don't notify
                }
                
                spdlog::info("Current version: {}, Latest version: {}, Has update: {}", 
                           currentVersion.toString(), release.version.toString(), hasUpdate);
                
                // Call callback on main thread
                callback(hasUpdate, release);
            } else {
                spdlog::error("Failed to check for updates: HTTP {}", response.status_code);
                ReleaseInfo empty;
                callback(false, empty);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error checking for updates: {}", e.what());
            ReleaseInfo empty;
            callback(false, empty);
        }
        
        pImpl->isCheckingForUpdates = false; })
            .detach();
    }

    void UpdateChecker::showUpdateDialog(const ReleaseInfo &release, std::function<void(UpdateAction)> callback)
    {
        pImpl->showUpdateDialogFlag = true;
        pImpl->pendingRelease = release;
        pImpl->pendingCallback = callback;
    }

    ReleaseInfo UpdateChecker::parseReleaseInfo(const nlohmann::json &releaseJson)
    {
        ReleaseInfo info;

        info.tagName = releaseJson.value("tag_name", "");
        info.name = releaseJson.value("name", "");
        info.htmlUrl = releaseJson.value("html_url", "");
        info.isPrerelease = releaseJson.value("prerelease", false);
        info.version = Version::fromString(info.tagName);

#ifdef _WIN32
        // Find Windows installer asset
        if (releaseJson.contains("assets") && releaseJson["assets"].is_array())
        {
            info.downloadUrl = findWindowsAsset(releaseJson["assets"], info.tagName);
        }
#endif

        return info;
    }

    std::string UpdateChecker::findWindowsAsset(const nlohmann::json &assets, const std::string &version)
    {
        std::string targetAsset = "led-matrix-desktop-" + version + "-win64.exe";

        for (const auto &asset : assets)
        {
            std::string name = asset.value("name", "");
            if (name == targetAsset)
            {
                return asset.value("browser_download_url", "");
            }
        }

        return "";
    }

#ifdef _WIN32
    void UpdateChecker::downloadAndInstallUpdate(const ReleaseInfo &release, std::function<void(bool success, const std::string &error)> callback)
    {
        if (release.downloadUrl.empty())
        {
            callback(false, "No download URL found for Windows installer");
            return;
        }

        std::thread([this, release, callback]()
                    {
        try {
            // Create temp directory
            std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "led-matrix-update";
            std::filesystem::create_directories(tempDir);
            
            std::filesystem::path installerPath = tempDir / ("led-matrix-installer-" + release.tagName + ".exe");
            
            spdlog::info("Downloading installer from: {}", release.downloadUrl);
            spdlog::info("Saving to: {}", installerPath.string());
            
            // Download the installer
            HRESULT hr = URLDownloadToFileA(nullptr, release.downloadUrl.c_str(), 
                                          installerPath.string().c_str(), 0, nullptr);
            
            if (SUCCEEDED(hr)) {
                spdlog::info("Download completed successfully");
                
                // Execute the installer
                SHELLEXECUTEINFOA sei = {};
                sei.cbSize = sizeof(SHELLEXECUTEINFOA);
                sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                sei.lpVerb = "runas"; // Run as administrator
                sei.lpFile = installerPath.string().c_str();
                sei.nShow = SW_NORMAL;
                
                if (ShellExecuteExA(&sei)) {
                    spdlog::info("Installer launched successfully");
                    
                    // Schedule application exit
                    HelloImGui::GetRunnerParams()->appShallExit = true;
                    
                    callback(true, "");
                } else {
                    DWORD error = GetLastError();
                    std::string errorMsg = "Failed to launch installer. Error code: " + std::to_string(error);
                    spdlog::error(errorMsg);
                    callback(false, errorMsg);
                }
            } else {
                std::string errorMsg = "Failed to download installer. HRESULT: " + std::to_string(hr);
                spdlog::error(errorMsg);
                callback(false, errorMsg);
            }
        } catch (const std::exception& e) {
            std::string errorMsg = "Exception during update: " + std::string(e.what());
            spdlog::error(errorMsg);
            callback(false, errorMsg);
        } })
            .detach();
    }
#else
    void UpdateChecker::downloadAndInstallUpdate(const ReleaseInfo &release, std::function<void(bool success, const std::string &error)> callback)
    {
        // On Linux, just open the releases page
        openReleasesPage(release);
        callback(true, "");
    }
#endif

    void UpdateChecker::openReleasesPage(const ReleaseInfo &release)
    {
        std::string url = release.htmlUrl.empty() ? "https://github.com/sshcrack/led-matrix/releases" : release.htmlUrl;

#ifdef _WIN32
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        std::string command = "xdg-open \"" + url + "\"";
        int result = std::system(command.c_str());
        if (result != 0)
        {
            spdlog::warn("Failed to open browser with xdg-open, trying alternatives");
            // Try alternative browsers
            std::vector<std::string> browsers = {"firefox", "google-chrome", "chromium-browser", "brave-browser"};
            for (const auto &browser : browsers)
            {
                command = browser + " \"" + url + "\" &";
                if (std::system(command.c_str()) == 0)
                {
                    break;
                }
            }
        }
#endif

        spdlog::info("Opened releases page: {}", url);
    }

} // namespace UpdateChecker

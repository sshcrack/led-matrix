#pragma once

#include <string>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace UpdateChecker
{

    struct SHARED_DESKTOP_API Version
    {
        int major = 0;
        int minor = 0;
        int patch = 0;

        Version() = default;
        Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

        // Parse version from string like "v1.2.3" or "1.2.3"
        static Version fromString(const std::string &versionStr);

        // Compare versions
        bool operator>(const Version &other) const;
        bool operator==(const Version &other) const;
        bool operator<(const Version &other) const;

        std::string toString() const;
    };

    struct SHARED_DESKTOP_API ReleaseInfo
    {
        std::string tagName;
        std::string name;
        std::string htmlUrl;
        std::string downloadUrl; // For Windows installer
        Version version;
        bool isPrerelease = false;
    };

    enum class SHARED_DESKTOP_API UpdateAction
    {
        UpdateNow,
        RemindLater,
        Skip
    };

    struct SHARED_DESKTOP_API UpdatePreferences
    {
        std::string skippedVersion;
        int64_t lastRemindTime = 0;
        bool updateNotificationsEnabled = true;
        int remindIntervalDays = 7; // Remind every 7 days

        void save();
        void load();
        bool shouldRemindForVersion(const std::string &version) const;
        bool shouldSkipVersion(const std::string &version) const;
    };

    class SHARED_DESKTOP_API UpdateChecker
    {
    public:
        UpdateChecker();
        ~UpdateChecker();

        // Check for updates asynchronously
        void checkForUpdates(std::function<void(bool hasUpdate, const ReleaseInfo &release)> callback);

        // Show update dialog and handle user choice
        void showUpdateDialog(const ReleaseInfo &release, std::function<void(UpdateAction)> callback);

        // Get current application version
        static Version getCurrentVersion();

        // Get and set update preferences
        UpdatePreferences &getPreferences();

        // Download and install update (Windows only)
        void downloadAndInstallUpdate(const ReleaseInfo &release, std::function<void(bool success, const std::string &error)> callback);

        // Open browser to releases page (Linux)
        void openReleasesPage(const ReleaseInfo &release);

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;

        // Internal methods
        ReleaseInfo parseReleaseInfo(const nlohmann::json &releaseJson);
        std::string findWindowsAsset(const nlohmann::json &assets, const std::string &version);
    };

} // namespace UpdateChecker

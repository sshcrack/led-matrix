#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "shared/desktop/macro.h"
#include "shared/common/Version.h"

// Network side of desktop updates: GitHub releases and the matrix's update API.
// All calls block; UpdateManager runs them on a worker thread.
namespace UpdateChecker
{
    struct SHARED_DESKTOP_API ReleaseInfo
    {
        std::string tagName;
        std::string name;
        std::string htmlUrl;
        std::string downloadUrl; // this platform's package (tar.gz / zip)
        std::string sha256;      // from the asset's "digest"; empty when GitHub has none
        Common::Version version;
        bool isPrerelease = false;
    };

    /// What the matrix reports on GET /api/update/status.
    struct SHARED_DESKTOP_API MatrixUpdateState
    {
        Common::Version version;
        bool autoUpdateEnabled = false;
        bool busy = false; // checking, downloading or installing an update
    };

    struct SHARED_DESKTOP_API UpdatePreferences
    {
        std::string skippedVersion;
        int64_t lastRemindTime = 0;
        bool updateNotificationsEnabled = true;
        int remindIntervalDays = 7;
        /// Download and install updates without asking (per-user installs only).
        bool autoUpdate = true;
        /// Run the same release as the matrix (needed for desktop render offload,
        /// which requires matching scene-code fingerprints). Asks the matrix to
        /// update first when a newer release is out and its auto-update is on.
        bool followMatrix = true;

        void save();
        void load();
        bool shouldRemindForVersion(const std::string &version) const;
        bool shouldSkipVersion(const std::string &version) const;
    };

    class SHARED_DESKTOP_API UpdateChecker
    {
    public:
        static Common::Version getCurrentVersion();

        /// The latest release, or the release of `version` when given.
        static std::expected<ReleaseInfo, std::string> fetchRelease(const std::optional<Common::Version> &version = std::nullopt);

        static std::expected<MatrixUpdateState, std::string> fetchMatrixState(const std::string &host, int port);
        /// Asks the matrix to install `version` (POST /api/update/install).
        static std::expected<void, std::string> requestMatrixUpdate(const std::string &host, int port,
                                                                    const Common::Version &version);

        static ReleaseInfo parseReleaseInfo(const nlohmann::json &releaseJson);
    };

} // namespace UpdateChecker

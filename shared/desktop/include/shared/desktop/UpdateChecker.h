#pragma once

#include <cstdint>
#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "shared/common/Releases.h"
#include "shared/common/Version.h"
#include "shared/desktop/macro.h"

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
        /// Absent when the matrix reports none (a status this build does not know).
        std::optional<Releases::UpdateStatus> status;
        bool busy = false; // checking, downloading or installing an update
    };

    /// Why a matrix status request failed. `Unreachable` and `Http` are network problems, `Parse` means the
    /// matrix answered with something this desktop cannot read.
    struct SHARED_DESKTOP_API MatrixFetchError {
        enum class Kind { Unreachable, Http, Parse };
        Kind kind = Kind::Unreachable;
        std::string message;
    };

    struct SHARED_DESKTOP_API UpdatePreferences
    {
        std::string skippedVersion;
        int64_t lastRemindTime = 0;
        bool updateNotificationsEnabled = true;
        int remindIntervalDays = 7;
        /// Download and install updates without asking (per-user installs only).
        bool autoUpdate = true;
        /// Follow the matrix release to keep features and data contracts aligned.
        /// Offload itself checks protocol and per-scene contracts. Asks the matrix to
        /// update first when a newer release is out and its auto-update is on.
        bool followMatrix = true;

        void save();
        void load();
        bool shouldRemindForVersion(const std::string &version) const;
        bool shouldSkipVersion(const std::string &version) const;
    };

    /// Where to reach the matrix. A copy taken on the main thread, so background checks
    /// never read the config (which shutdown destroys while they may still run).
    struct SHARED_DESKTOP_API MatrixEndpoint
    {
        std::string host;
        int port = 0;
        std::string authorization; ///< Authorization header value, empty for none

        static MatrixEndpoint fromConfig();
    };

    class SHARED_DESKTOP_API UpdateChecker
    {
    public:
        static Common::Version getCurrentVersion();

        /// The latest release, or the release of `version` when given.
        static std::expected<ReleaseInfo, std::string> fetchRelease(const std::optional<Common::Version> &version = std::nullopt);

        /// GET /api/update/status. The one reader of that response (the version check in the settings uses it too).
        static std::expected<MatrixUpdateState, MatrixFetchError> fetchMatrixState(const MatrixEndpoint& matrix);
        /// Reads a status body: the status by name (`status_name`) or, from an older matrix, by its int.
        static std::expected<MatrixUpdateState, std::string> parseMatrixState(const nlohmann::json& status);
        /// Asks the matrix to install `version` (POST /api/update/install).
        static std::expected<void, std::string> requestMatrixUpdate(const MatrixEndpoint &matrix,
                                                                    const Common::Version &version);

        /// This platform's package out of a GitHub release JSON object, via Releases. `downloadUrl` stays empty when
        /// the release has no such asset or it is not on an allowed host; `sha256` stays empty without a digest
        /// (SelfUpdate::download_and_install refuses to install it).
        static ReleaseInfo parseReleaseInfo(const nlohmann::json &releaseJson);
        static ReleaseInfo toReleaseInfo(const Releases::Release& release);
    };

} // namespace UpdateChecker

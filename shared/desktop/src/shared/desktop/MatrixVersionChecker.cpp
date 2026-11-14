#include "shared/desktop/MatrixVersionChecker.h"

#include <spdlog/spdlog.h>

#include <thread>

#include "shared/desktop/UpdateChecker.h"
#include "shared/desktop/config.h"

namespace MatrixVersionChecker
{
    struct MatrixVersionChecker::Impl
    {
        bool isChecking = false;
    };

    MatrixVersionChecker::MatrixVersionChecker() : pImpl(std::make_unique<Impl>()) {}

    MatrixVersionChecker::~MatrixVersionChecker() = default;

    const Common::Version& MatrixVersionChecker::getDesktopVersion()
    {
        return Common::Version::getCurrentVersion();
    }

    VersionCompatibility MatrixVersionChecker::compareVersions(
        const Common::Version& desktopVersion,
        const Common::Version& matrixVersion)
    {
        // Check if major.minor versions are compatible
        if (desktopVersion.isCompatibleWith(matrixVersion))
        {
            return VersionCompatibility::Compatible;
        }
        
        // Compare major and minor versions to determine who needs to update
        if (desktopVersion.major > matrixVersion.major || 
            (desktopVersion.major == matrixVersion.major && desktopVersion.minor > matrixVersion.minor))
        {
            return VersionCompatibility::DesktopNewer; // Matrix needs update
        }
        else
        {
            return VersionCompatibility::MatrixNewer;  // Desktop needs update
        }
    }

    void MatrixVersionChecker::checkMatrixVersion(
        const std::string& hostname, 
        int port,
        std::function<void(const MatrixVersionInfo&)> callback)
    {
        if (pImpl->isChecking)
        {
            spdlog::warn("Matrix version check already in progress");
            return;
        }

        pImpl->isChecking = true;

        // Run HTTP request in background thread
        std::thread([this, hostname, port, callback]()
        {
            MatrixVersionInfo result;
            
            try 
            {
                spdlog::info("Checking matrix version at {}:{}", hostname, port);
                // The one reader of GET /api/update/status, shared with the desktop updater.
                const auto state = UpdateChecker::UpdateChecker::fetchMatrixState({hostname, port, Config::matrixAuthorizationHeader()});
                if (state) {
                    result.version = state->version;
                    const Common::Version desktopVersion = getDesktopVersion();
                    result.compatibility = compareVersions(desktopVersion, result.version);

                    spdlog::info("Matrix version: {}, Desktop version: {}, Compatibility: {}", result.version.toString(),
                                 desktopVersion.toString(), static_cast<int>(result.compatibility));
                }
                else {
                    using Kind = UpdateChecker::MatrixFetchError::Kind;
                    result.compatibility =
                        state.error().kind == Kind::Parse ? VersionCompatibility::ParseError : VersionCompatibility::NetworkError;
                    result.errorMessage = state.error().message;
                    spdlog::error("Matrix version check failed: {}", result.errorMessage);
                }
            }
            catch (const std::exception& e) 
            {
                result.compatibility = VersionCompatibility::ParseError;
                result.errorMessage = "Exception: " + std::string(e.what());
                spdlog::error("Matrix version check failed: {}", result.errorMessage);
            }
            
            // Call callback on main thread (note: this is actually still background thread,
            // but the GUI will handle it properly)
            callback(result);
            
            pImpl->isChecking = false;
        }).detach();
    }

} // namespace MatrixVersionChecker
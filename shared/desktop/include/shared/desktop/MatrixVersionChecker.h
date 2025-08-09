#pragma once

#include "shared/desktop/macro.h"
#include "shared/common/Version.h"
#include <string>
#include <functional>
#include <memory>

namespace MatrixVersionChecker
{
    enum class SHARED_DESKTOP_API VersionCompatibility
    {
        Compatible,           // Desktop and matrix versions are compatible (major.minor match)
        DesktopNewer,        // Desktop is newer than matrix - matrix needs update
        MatrixNewer,         // Matrix is newer than desktop - desktop needs update
        NetworkError,        // Could not reach matrix
        ParseError          // Could not parse version response
    };

    struct SHARED_DESKTOP_API MatrixVersionInfo
    {
        Common::Version version;
        VersionCompatibility compatibility;
        std::string errorMessage;
    };

    class SHARED_DESKTOP_API MatrixVersionChecker
    {
    public:
        MatrixVersionChecker();
        ~MatrixVersionChecker();

        // Check matrix version asynchronously
        void checkMatrixVersion(
            const std::string& hostname, 
            int port,
            std::function<void(const MatrixVersionInfo&)> callback
        );

        // Get current desktop version
        static const Common::Version& getDesktopVersion();

        // Check if versions are compatible
        static VersionCompatibility compareVersions(
            const Common::Version& desktopVersion,
            const Common::Version& matrixVersion
        );

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace MatrixVersionChecker
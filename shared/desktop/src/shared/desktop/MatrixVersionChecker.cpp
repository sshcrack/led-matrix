#include "shared/desktop/MatrixVersionChecker.h"
#include <spdlog/spdlog.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>

namespace MatrixVersionChecker
{
    struct MatrixVersionChecker::Impl
    {
        bool isChecking = false;
    };

    MatrixVersionChecker::MatrixVersionChecker() : pImpl(std::make_unique<Impl>()) {}

    MatrixVersionChecker::~MatrixVersionChecker() = default;

    Common::Version MatrixVersionChecker::getDesktopVersion()
    {
        // Use compile-time definitions from CMakeLists.txt
        return Common::Version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
    }

    VersionCompatibility MatrixVersionChecker::compareVersions(
        const Common::Version& desktopVersion,
        const Common::Version& matrixVersion)
    {
        if (desktopVersion > matrixVersion)
        {
            return VersionCompatibility::DesktopNewer;
        }
        else if (matrixVersion > desktopVersion)
        {
            return VersionCompatibility::MatrixNewer;
        }
        else
        {
            return VersionCompatibility::Compatible;
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
                std::string url = "http://" + hostname + ":" + std::to_string(port) + "/api/update/status";
                spdlog::info("Checking matrix version from: {}", url);
                
                auto response = cpr::Get(
                    cpr::Url{url},
                    cpr::Header{{"User-Agent", "led-matrix-desktop/" + getDesktopVersion().toString()}},
                    cpr::Timeout{5000} // 5 second timeout
                );
                
                if (response.status_code == 200) 
                {
                    auto statusJson = nlohmann::json::parse(response.text);
                    
                    if (statusJson.contains("current_version")) 
                    {
                        std::string versionStr = statusJson["current_version"];
                        result.version = Common::Version::fromString(versionStr);
                        
                        Common::Version desktopVersion = getDesktopVersion();
                        result.compatibility = compareVersions(desktopVersion, result.version);
                        
                        spdlog::info("Matrix version: {}, Desktop version: {}, Compatibility: {}", 
                                   result.version.toString(), 
                                   desktopVersion.toString(),
                                   static_cast<int>(result.compatibility));
                    }
                    else 
                    {
                        result.compatibility = VersionCompatibility::ParseError;
                        result.errorMessage = "Response does not contain current_version field";
                        spdlog::error("Matrix version check failed: {}", result.errorMessage);
                    }
                } 
                else if (response.status_code == 0) 
                {
                    result.compatibility = VersionCompatibility::NetworkError;
                    result.errorMessage = "Could not connect to matrix (network error)";
                    spdlog::error("Matrix version check failed: {}", result.errorMessage);
                }
                else 
                {
                    result.compatibility = VersionCompatibility::NetworkError;
                    result.errorMessage = "HTTP error " + std::to_string(response.status_code);
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
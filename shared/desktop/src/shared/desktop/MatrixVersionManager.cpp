#include "shared/desktop/MatrixVersionManager.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

namespace MatrixVersionChecker
{
    struct MatrixVersionManager::Impl
    {
        MatrixVersionChecker checker;
        bool showIncompatibleDialog = false;
        bool isCheckingVersion = false;
        MatrixVersionInfo lastResult;
        std::string lastHostname;
        int lastPort = 0;

        // Dialog state
        bool userAcknowledged = false;
    };

    MatrixVersionManager::MatrixVersionManager() : pImpl(std::make_unique<Impl>()) {}

    MatrixVersionManager::~MatrixVersionManager() = default;

    void MatrixVersionManager::render(ImGuiContext* ctx)
    {
        ImGui::SetCurrentContext(ctx);

        // Render the incompatible version dialog if needed
        if (pImpl->showIncompatibleDialog)
        {
            renderIncompatibleVersionDialog();
        }
    }

    void MatrixVersionManager::checkMatrixVersionAsync(const std::string& hostname, int port)
    {
        if (pImpl->isCheckingVersion)
        {
            return;
        }

        pImpl->lastHostname = hostname;
        pImpl->lastPort = port;
        pImpl->isCheckingVersion = true;
        pImpl->userAcknowledged = false;

        spdlog::info("Starting matrix version check for {}:{}", hostname, port);

        pImpl->checker.checkMatrixVersion(hostname, port, [this](const MatrixVersionInfo& result)
        {
            pImpl->isCheckingVersion = false;
            handleVersionCheckResult(result);
        });
    }

    bool MatrixVersionManager::isCompatible() const
    {
        return pImpl->lastResult.compatibility == VersionCompatibility::Compatible ||
               pImpl->lastResult.compatibility == VersionCompatibility::MatrixNewer ||
               pImpl->userAcknowledged;
    }

    MatrixVersionInfo MatrixVersionManager::getLastCheckResult() const
    {
        return pImpl->lastResult;
    }

    void MatrixVersionManager::clearDialogs()
    {
        pImpl->showIncompatibleDialog = false;
        pImpl->userAcknowledged = false;
    }

    void MatrixVersionManager::handleVersionCheckResult(const MatrixVersionInfo& result)
    {
        pImpl->lastResult = result;

        switch (result.compatibility)
        {
        case VersionCompatibility::Compatible:
            spdlog::info("Matrix version is compatible");
            break;

        case VersionCompatibility::MatrixNewer:
            spdlog::info("Matrix version is newer than desktop - this is allowed");
            break;

        case VersionCompatibility::DesktopNewer:
            spdlog::warn("Desktop version is newer than matrix - showing upgrade prompt");
            pImpl->showIncompatibleDialog = true;
            break;

        case VersionCompatibility::NetworkError:
            spdlog::warn("Could not check matrix version due to network error: {}", result.errorMessage);
            // Don't block the user for network errors - they might be developing offline
            break;

        case VersionCompatibility::ParseError:
            spdlog::error("Could not parse matrix version response: {}", result.errorMessage);
            // Don't block the user for parse errors - matrix might be older version without the endpoint
            break;
        }
    }

    void MatrixVersionManager::renderIncompatibleVersionDialog()
    {
        ImGui::OpenPopup("Matrix Version Compatibility");

        if (ImGui::BeginPopupModal("Matrix Version Compatibility", nullptr, 
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("⚠️ Version Compatibility Issue");
            ImGui::Separator();

            ImGui::Text("Your desktop application is newer than the matrix controller:");
            ImGui::Spacing();

            ImGui::Text("Desktop version:  v%s", MatrixVersionChecker::getDesktopVersion().toString().c_str());
            ImGui::Text("Matrix version:   v%s", pImpl->lastResult.version.toString().c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextWrapped("To ensure optimal compatibility and access to the latest features, "
                             "please update your matrix controller to match the desktop version.");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), 
                              "You can continue using the current setup, but some features may not work correctly.");

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Update Matrix Controller", ImVec2(200, 0)))
            {
                // Open the matrix web interface to the updates page
                std::string url = "http://" + pImpl->lastHostname + ":" + std::to_string(pImpl->lastPort) + "/web/#/updates";
                
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
                std::string command = "xdg-open \"" + url + "\"";
                int result = std::system(command.c_str());
                if (result != 0)
                {
                    spdlog::warn("Failed to open browser with xdg-open, trying alternatives");
                    std::vector<std::string> browsers = {"firefox", "google-chrome", "chromium-browser", "brave-browser"};
                    for (const auto& browser : browsers)
                    {
                        command = browser + " \"" + url + "\" &";
                        if (std::system(command.c_str()) == 0)
                        {
                            break;
                        }
                    }
                }
#endif
                spdlog::info("Opened matrix updates page: {}", url);
            }

            ImGui::SameLine();
            if (ImGui::Button("Continue Anyway", ImVec2(150, 0)))
            {
                pImpl->userAcknowledged = true;
                pImpl->showIncompatibleDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("You can recheck the version after updating the matrix");

            ImGui::EndPopup();
        }
    }

} // namespace MatrixVersionChecker
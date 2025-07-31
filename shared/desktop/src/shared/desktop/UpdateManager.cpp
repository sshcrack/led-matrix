#include "shared/desktop/UpdateManager.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <shared/desktop/config.h>
#include <chrono>

namespace UpdateChecker
{

    struct UpdateManager::Impl
    {
        UpdateChecker checker;
        bool showUpdateDialog = false;
        bool showUpToDateDialog = false;
        bool isCheckingForUpdates = false;
        bool updateNotificationsEnabled = true;
        bool showDownloadProgress = false;
        bool downloadInProgress = false;
        bool isManualCheck = false;
        std::string downloadError;
        ReleaseInfo currentRelease;

        // Dialog state
        bool userWantsUpdate = false;
        bool userDismissed = false;
    };

    UpdateManager::UpdateManager() : pImpl(std::make_unique<Impl>())
    {
        // Load notification preference from update checker preferences
        pImpl->updateNotificationsEnabled = pImpl->checker.getPreferences().updateNotificationsEnabled;
    }

    UpdateManager::~UpdateManager() = default;

    void UpdateManager::render()
    {
        // Render the update dialog if needed
        if (pImpl->showUpdateDialog)
        {
            renderUpdateDialog();
        }

        // Render download progress if needed
        if (pImpl->showDownloadProgress)
        {
            ImGui::OpenPopup("Download Progress");

            if (ImGui::BeginPopupModal("Download Progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
            {
                if (pImpl->downloadInProgress)
                {
                    ImGui::Text("Downloading and installing update...");
                    ImGui::Text("Please wait, this may take a few moments.");

                    // Show spinner
                    static float progress = 0.0f;
                    static int progressDir = 1;
                    progress += progressDir * 0.02f;
                    if (progress >= 1.0f)
                    {
                        progress = 1.0f;
                        progressDir *= -1;
                    }
                    if (progress <= 0.0f)
                    {
                        progress = 0.0f;
                        progressDir *= -1;
                    }

                    ImGui::ProgressBar(progress, ImVec2(300.0f, 0.0f));
                    ImGui::Text("The application will close when the installer starts.");
                }
                else
                {
                    if (!pImpl->downloadError.empty())
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Download failed:");
                        ImGui::TextWrapped("%s", pImpl->downloadError.c_str());

                        if (ImGui::Button("OK", ImVec2(120, 0)))
                        {
                            pImpl->showDownloadProgress = false;
                            pImpl->downloadError.clear();
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    else
                    {
                        ImGui::Text("Download completed successfully!");
                        ImGui::Text("The installer should start shortly.");

                        if (ImGui::Button("OK", ImVec2(120, 0)))
                        {
                            pImpl->showDownloadProgress = false;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }
        
        // Render "up to date" dialog if needed
        if (pImpl->showUpToDateDialog) {
            ImGui::OpenPopup("Software Up to Date");
            
            if (ImGui::BeginPopupModal("Software Up to Date", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::Text("✅ You're running the latest version!");
                ImGui::Separator();
                
                ImGui::Text("Current version: v%s", UpdateChecker::getCurrentVersion().toString().c_str());
                ImGui::Text("No updates are available at this time.");
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Check back later or enable automatic notifications in the menu");
                
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    pImpl->showUpToDateDialog = false;
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
        }
    }

    void UpdateManager::checkForUpdatesAsync()
    {
        if (pImpl->isCheckingForUpdates || !pImpl->updateNotificationsEnabled)
        {
            return;
        }

        pImpl->isCheckingForUpdates = true;
        pImpl->isManualCheck = false;
        spdlog::info("Starting automatic update check...");

        pImpl->checker.checkForUpdates([this](bool hasUpdate, const ReleaseInfo &release)
                                       {
        pImpl->isCheckingForUpdates = false;
        handleUpdateCheckResult(hasUpdate, release); });
    }

    void UpdateManager::checkForUpdatesManual()
    {
        if (pImpl->isCheckingForUpdates)
        {
            return;
        }

        pImpl->isCheckingForUpdates = true;
        pImpl->isManualCheck = true;
        spdlog::info("Starting manual update check...");

        pImpl->checker.checkForUpdates([this](bool hasUpdate, const ReleaseInfo &release)
                                       {
        pImpl->isCheckingForUpdates = false;
        handleUpdateCheckResult(hasUpdate, release); });
    }

    void UpdateManager::setUpdateNotificationsEnabled(bool enabled)
    {
        pImpl->updateNotificationsEnabled = enabled;

        // Save to update checker preferences
        pImpl->checker.getPreferences().updateNotificationsEnabled = enabled;
        pImpl->checker.getPreferences().save();
    }

    bool UpdateManager::isUpdateNotificationsEnabled() const
    {
        return pImpl->updateNotificationsEnabled;
    }

    void UpdateManager::resetUpdatePreferences()
    {
        auto &prefs = pImpl->checker.getPreferences();
        prefs.skippedVersion = "";
        prefs.lastRemindTime = 0;
        prefs.updateNotificationsEnabled = true;
        prefs.remindIntervalDays = 7;
        prefs.save();

        pImpl->updateNotificationsEnabled = true;
        spdlog::info("Reset all update preferences to defaults");
    }

    UpdatePreferences &UpdateManager::getUpdatePreferences()
    {
        return pImpl->checker.getPreferences();
    }

    void UpdateManager::handleUpdateCheckResult(bool hasUpdate, const ReleaseInfo &release)
    {
        if (hasUpdate && (pImpl->updateNotificationsEnabled || pImpl->isManualCheck))
        {
            spdlog::info("Update available: {} -> {}",
                         UpdateChecker::getCurrentVersion().toString(),
                         release.version.toString());

            pImpl->currentRelease = release;
            pImpl->showUpdateDialog = true;
            pImpl->userWantsUpdate = false;
            pImpl->userDismissed = false;
        }
        else if (hasUpdate)
        {
            spdlog::info("Update available but notifications disabled");
        }
        else
        {
            spdlog::info("No updates available");
            // Show "up to date" dialog for manual checks
            if (pImpl->isManualCheck) {
                pImpl->showUpToDateDialog = true;
            }
        }
        
        // Reset manual check flag
        pImpl->isManualCheck = false;
    }

    void UpdateManager::handleUpdateAction(UpdateAction action, const ReleaseInfo &release)
    {
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        switch (action)
        {
        case UpdateAction::UpdateNow:
            spdlog::info("User chose to update now");
            pImpl->showDownloadProgress = true;
            pImpl->downloadInProgress = true;
            pImpl->downloadError.clear();

            pImpl->checker.downloadAndInstallUpdate(release, [this](bool success, const std::string &error)
                                                    {
                pImpl->downloadInProgress = false;
                if (!success) {
                    pImpl->downloadError = error;
                    spdlog::error("Update download/install failed: {}", error);
                } });
            break;

        case UpdateAction::RemindLater:
            spdlog::info("User chose to be reminded later for version: {}", release.tagName);
            {
                auto &prefs = pImpl->checker.getPreferences();
                prefs.lastRemindTime = nowTime;
                // Clear any skipped version since user wants to be reminded
                if (prefs.skippedVersion == release.tagName)
                {
                    prefs.skippedVersion = "";
                }
                prefs.save();
            }
            break;

        case UpdateAction::Skip:
            spdlog::info("User chose to skip version: {}", release.tagName);
            {
                auto &prefs = pImpl->checker.getPreferences();
                prefs.skippedVersion = release.tagName;
                prefs.lastRemindTime = nowTime;
                prefs.save();
            }
            break;
        }
    }

    void UpdateManager::renderUpdateDialog()
    {
        ImGui::OpenPopup("Update Available");

        if (ImGui::BeginPopupModal("Update Available", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            const auto &release = pImpl->currentRelease;

            ImGui::Text("🚀 A new version of LED Matrix Controller is available!");
            ImGui::Separator();

            ImGui::Text("Current version: v%s", UpdateChecker::getCurrentVersion().toString().c_str());
            ImGui::Text("Latest version:  %s", release.tagName.c_str());

            if (!release.name.empty() && release.name != release.tagName)
            {
                ImGui::Text("Release name: %s", release.name.c_str());
            }

            ImGui::Separator();
            ImGui::Spacing();

            // Add some helpful text based on platform
#ifdef _WIN32
            ImGui::TextWrapped("Click 'Update Now' to automatically download and install the latest version. "
                               "The application will close during the update process.");

            if (ImGui::Button("🔄 Update Now", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::UpdateNow, release);
                pImpl->showUpdateDialog = false;
                ImGui::CloseCurrentPopup();
            }
#else
            ImGui::TextWrapped("Click 'View Release' to open the GitHub releases page where you can "
                               "download the latest version manually.");

            if (ImGui::Button("🌐 View Release", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::UpdateNow, release); // On Linux this opens browser
                pImpl->showUpdateDialog = false;
                ImGui::CloseCurrentPopup();
            }
#endif

            ImGui::SameLine();
            if (ImGui::Button("⏰ Remind Later", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::RemindLater, release);
                pImpl->showUpdateDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemTooltip("You'll be reminded again in %d days",
                                  pImpl->checker.getPreferences().remindIntervalDays);

            ImGui::SameLine();
            if (ImGui::Button("❌ Skip Version", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::Skip, release);
                pImpl->showUpdateDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemTooltip("Don't notify about this version anymore");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("You can change notification preferences in the menu");

            ImGui::EndPopup();
        }
    }

} // namespace UpdateChecker

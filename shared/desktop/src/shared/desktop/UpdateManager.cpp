#pragma execution_character_set("utf-8")
#include "shared/desktop/UpdateManager.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <shared/desktop/config.h>
#include <chrono>
#include <mutex>


namespace UpdateChecker
{

    /// Update workflow. Exactly one phase is current; each phase owns one
    /// dialog, so impossible flag combinations cannot be represented.
    ///
    ///   Idle ──check──▶ Checking ──update──▶ Prompt ──update now──▶ Downloading
    ///                     │  └──none (manual)──▶ UpToDate            │      │
    ///                     └──none (automatic) / notifications off──▶ Idle   │
    ///   Downloading ──ok──▶ DownloadDone   Downloading ──fail──▶ DownloadFailed
    ///   Prompt/UpToDate/DownloadDone/DownloadFailed ──dismiss──▶ Idle
    ///
    /// The checker reports from its own threads, so `phase` and the data that
    /// belongs to it are guarded by `mutex`; render() works on a copy.
    enum class Phase { Idle, Checking, Prompt, UpToDate, Downloading, DownloadDone, DownloadFailed };
    enum class CheckKind { Automatic, Manual };

    struct UpdateManager::Impl
    {
        UpdateChecker checker;
        std::atomic<bool> updateNotificationsEnabled{true};

        std::mutex mutex;
        Phase phase = Phase::Idle;
        CheckKind checkKind = CheckKind::Automatic;
        ReleaseInfo currentRelease;  // Prompt
        std::string downloadError;   // DownloadFailed

        // Download progress tracking
        std::atomic<size_t> downloadedBytes{0};
        std::atomic<size_t> totalBytes{0};

        Phase getPhase()
        {
            std::lock_guard lock(mutex);
            return phase;
        }
        void dismiss(Phase expected)
        {
            std::lock_guard lock(mutex);
            if (phase == expected) phase = Phase::Idle;
        }
        /// Idle/terminal dialog → Checking. False while busy.
        bool beginCheck(CheckKind kind)
        {
            std::lock_guard lock(mutex);
            if (phase == Phase::Checking || phase == Phase::Downloading) return false;
            phase = Phase::Checking;
            checkKind = kind;
            return true;
        }
    };

    UpdateManager::UpdateManager() : pImpl(std::make_unique<Impl>())
    {
        // Load notification preference from update checker preferences
        pImpl->updateNotificationsEnabled = pImpl->checker.getPreferences().updateNotificationsEnabled;
    }

    UpdateManager::~UpdateManager() = default;

    void UpdateManager::render(ImGuiContext *ctx)
    {
        ImGui::SetCurrentContext(ctx);

        const Phase phase = pImpl->getPhase();

        // Render the update dialog if needed
        if (phase == Phase::Prompt)
        {
            renderUpdateDialog();
        }

        // Render download progress if needed
        if (phase == Phase::Downloading || phase == Phase::DownloadDone || phase == Phase::DownloadFailed)
        {
            ImGui::OpenPopup("Download Progress");

            if (ImGui::BeginPopupModal("Download Progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
            {
                if (phase == Phase::Downloading)
                {
                    ImGui::Text("Downloading and installing update...");
                    ImGui::Text("Please wait, this may take a few moments.");

                    float progress = 0.0f;
                    size_t total = pImpl->totalBytes.load();
                    size_t downloaded = pImpl->downloadedBytes.load();
                    if (total > 0) {
                        progress = static_cast<float>(downloaded) / static_cast<float>(total);
                    }
                    ImGui::ProgressBar(progress, ImVec2(300.0f, 0.0f), total > 0 ? (std::to_string(downloaded / 1024) + " KB / " + std::to_string(total / 1024) + " KB").c_str() : "Calculating...");
                    ImGui::Text("The application will close when the installer starts.");
                }
                else
                {
                    if (phase == Phase::DownloadFailed)
                    {
                        std::string error;
                        {
                            std::lock_guard lock(pImpl->mutex);
                            error = pImpl->downloadError;
                        }
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Download failed:");
                        ImGui::TextWrapped("%s", error.c_str());

                        if (ImGui::Button("OK", ImVec2(120, 0)))
                        {
                            pImpl->dismiss(Phase::DownloadFailed);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    else
                    {
                        ImGui::Text("Download completed successfully!");
                        ImGui::Text("The installer should start shortly.");

                        if (ImGui::Button("OK", ImVec2(120, 0)))
                        {
                            pImpl->dismiss(Phase::DownloadDone);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }
        
        // Render "up to date" dialog if needed
        if (phase == Phase::UpToDate) {
            ImGui::OpenPopup("Software Up to Date");
            
            if (ImGui::BeginPopupModal("Software Up to Date", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::Text("You're running the latest version!");
                ImGui::Separator();
                
                ImGui::Text("Current version: v%s", UpdateChecker::getCurrentVersion().toString().c_str());
                ImGui::Text("No updates are available at this time.");
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Check back later or enable automatic notifications in the menu");
                
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    pImpl->dismiss(Phase::UpToDate);
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
        }
    }

    void UpdateManager::checkForUpdatesAsync()
    {
        if (!pImpl->updateNotificationsEnabled || !pImpl->beginCheck(CheckKind::Automatic))
        {
            return;
        }
        spdlog::info("Starting automatic update check...");

        pImpl->checker.checkForUpdates([this](bool hasUpdate, const ReleaseInfo &release)
                                       { handleUpdateCheckResult(hasUpdate, release); });
    }

    void UpdateManager::checkForUpdatesManual()
    {
        if (!pImpl->beginCheck(CheckKind::Manual))
        {
            return;
        }
        spdlog::info("Starting manual update check...");

        pImpl->checker.checkForUpdates([this](bool hasUpdate, const ReleaseInfo &release)
                                       { handleUpdateCheckResult(hasUpdate, release); });
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

    bool UpdateManager::shallAppExit() const {
        return pImpl->checker.shallAppExit();
    }

    void UpdateManager::handleUpdateCheckResult(bool hasUpdate, const ReleaseInfo &release)
    {
        std::lock_guard lock(pImpl->mutex);
        if (pImpl->phase != Phase::Checking)
            return;
        const bool manual = pImpl->checkKind == CheckKind::Manual;

        if (hasUpdate && (pImpl->updateNotificationsEnabled || manual))
        {
            spdlog::info("Update available: {} -> {}",
                         UpdateChecker::getCurrentVersion().toString(),
                         release.version.toString());

            pImpl->currentRelease = release;
            pImpl->phase = Phase::Prompt;
        }
        else if (hasUpdate)
        {
            spdlog::info("Update available but notifications disabled");
            pImpl->phase = Phase::Idle;
        }
        else
        {
            spdlog::info("No updates available");
            // Show "up to date" dialog for manual checks
            pImpl->phase = manual ? Phase::UpToDate : Phase::Idle;
        }
    }

    void UpdateManager::handleUpdateAction(UpdateAction action, const ReleaseInfo &release)
    {
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        switch (action)
        {
        case UpdateAction::UpdateNow:
            spdlog::info("User chose to update now");
            {
                std::lock_guard lock(pImpl->mutex);
                pImpl->phase = Phase::Downloading;
                pImpl->downloadError.clear();
            }
            pImpl->downloadedBytes = 0;
            pImpl->totalBytes = 0;

            pImpl->checker.downloadAndInstallUpdate(release, [this](bool success, const std::string &error) {
                std::lock_guard lock(pImpl->mutex);
                if (pImpl->phase != Phase::Downloading)
                    return;
                if (success) {
                    pImpl->phase = Phase::DownloadDone;
                } else {
                    pImpl->downloadError = error;
                    pImpl->phase = Phase::DownloadFailed;
                    spdlog::error("Update download/install failed: {}", error);
                }
            },
            // Progress callback
            [this](size_t downloaded, size_t total) {
                pImpl->downloadedBytes = downloaded;
                pImpl->totalBytes = total;
            });
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
            ReleaseInfo release;
            {
                std::lock_guard lock(pImpl->mutex);
                release = pImpl->currentRelease;
            }

            ImGui::Text("A new version of LED Matrix Controller is available!");
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

            if (ImGui::Button("Update Now", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::UpdateNow, release);
                ImGui::CloseCurrentPopup();
            }
#else
            ImGui::TextWrapped("Click 'View Release' to open the GitHub releases page where you can "
                               "download the latest version manually.");

            if (ImGui::Button("View Release", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::UpdateNow, release); // On Linux this opens browser
                ImGui::CloseCurrentPopup();
            }
#endif

            ImGui::SameLine();
            if (ImGui::Button("Remind Later", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::RemindLater, release);
                pImpl->dismiss(Phase::Prompt);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemTooltip("You'll be reminded again in %d days",
                                  pImpl->checker.getPreferences().remindIntervalDays);

            ImGui::SameLine();
            if (ImGui::Button("Skip Version", ImVec2(120, 0)))
            {
                handleUpdateAction(UpdateAction::Skip, release);
                pImpl->dismiss(Phase::Prompt);
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

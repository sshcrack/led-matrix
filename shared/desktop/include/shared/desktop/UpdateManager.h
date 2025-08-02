#pragma once
#pragma execution_character_set("utf-8")

#include <imgui_internal.h>

#include "UpdateChecker.h"
#include <memory>
#include <string>

namespace UpdateChecker
{

    class SHARED_DESKTOP_API UpdateManager
    {
    public:
        UpdateManager();
        ~UpdateManager();

        // Call this in the main GUI loop to render update dialogs
        void render(ImGuiContext *ctx);

        // Call this to start checking for updates (typically on app startup)
        void checkForUpdatesAsync();
        
        // Call this for manual update checks (shows "up to date" message if no updates)
        void checkForUpdatesManual();

        // Set whether to show update notifications
        void setUpdateNotificationsEnabled(bool enabled);
        bool isUpdateNotificationsEnabled() const;

        // Reset all update preferences (clear skipped versions, etc.)
        void resetUpdatePreferences();

        // Get update preferences for advanced configuration
        UpdatePreferences &getUpdatePreferences();

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;

        void handleUpdateCheckResult(bool hasUpdate, const ReleaseInfo &release);
        void handleUpdateAction(UpdateAction action, const ReleaseInfo &release);
        void renderUpdateDialog();
    };

} // namespace UpdateChecker

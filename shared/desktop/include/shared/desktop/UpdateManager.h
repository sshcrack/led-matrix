#pragma once
#pragma execution_character_set("utf-8")

#include <imgui_internal.h>

#include "UpdateChecker.h"
#include <memory>
#include <string>

namespace UpdateChecker
{
    /// Keeps the desktop app on the same release as the matrix (or the latest
    /// release when no matrix is reachable). Per-user installs update silently
    /// and restart on their own while hidden in the tray; others get a prompt
    /// that moves them to a per-user install (see SelfUpdate.h).
    class SHARED_DESKTOP_API UpdateManager
    {
    public:
        UpdateManager();
        ~UpdateManager();

        /// Renders the update dialogs; call once per frame from the GUI.
        void render(ImGuiContext *ctx);

        /// Drives periodic checks and the restart; call every frame (also while hidden).
        /// `windowHidden`: restart right away when an installed update is waiting.
        void tick(bool windowHidden);

        void checkForUpdatesAsync();  // automatic: silent unless a prompt is needed
        void checkForUpdatesManual(); // shows the outcome

        void setUpdateNotificationsEnabled(bool enabled);
        bool isUpdateNotificationsEnabled() const;
        void setAutoUpdateEnabled(bool enabled);
        bool isAutoUpdateEnabled() const;
        void setFollowMatrixEnabled(bool enabled);
        bool isFollowMatrixEnabled() const;

        void resetUpdatePreferences();
        UpdatePreferences &getUpdatePreferences();

        /// An update is installed and waits for a restart (its version, else empty).
        std::string pendingRestartVersion() const;
        void requestRestart(bool startMinimized);

        /// The app should exit (to restart into the new version).
        bool shallAppExit() const;
        /// After shutdown: restart into the installed update if one was requested.
        bool restartAfterExit() const;
        bool restartMinimized() const;

        /// One-line state for the menu ("Waiting for the matrix to update to 1.2.3", errors, ...).
        std::string statusLine() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace UpdateChecker

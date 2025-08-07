#pragma once

#include "shared/desktop/macro.h"
#include "MatrixVersionChecker.h"
#include <memory>
#include <imgui.h>

namespace MatrixVersionChecker
{
    class SHARED_DESKTOP_API MatrixVersionManager
    {
    public:
        MatrixVersionManager();
        ~MatrixVersionManager();

        // Call this in the main GUI loop to render version dialogs
        void render(ImGuiContext* ctx);

        // Check matrix version (typically when connecting to WebSocket)
        void checkMatrixVersionAsync(const std::string& hostname, int port);

        // Check if the system is compatible (no blocking dialogs)
        bool isCompatible() const;

        // Get last check result
        MatrixVersionInfo getLastCheckResult() const;

        // Clear any blocking dialogs (for debugging/testing)
        void clearDialogs();

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;

        void handleVersionCheckResult(const MatrixVersionInfo& result);
        void renderIncompatibleVersionDialog();
    };

} // namespace MatrixVersionChecker
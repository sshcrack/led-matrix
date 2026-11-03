#pragma once

// Private header shared by the src_desktop translation units.
// Declares the file-scope state that used to live at the top of main.cpp.

// Windows: WebsocketClient.h must come first so ixwebsocket pulls in winsock2/windows.h
// (with the USER API) before PluginLoader.h's win_compat.h defines NOUSER.
#include <shared/desktop/WebsocketClient.h>

#include <atomic>
#include <csignal>
#include <memory>

#include <shared/desktop/MatrixVersionManager.h>
#include <shared/desktop/UpdateManager.h>

namespace desktop_app::detail {
    // tray_window.cpp
    extern volatile sig_atomic_t shouldExit;
    extern std::atomic<bool> showMainWindow;
    extern bool startMinimized;
    extern int startupFrameCounter;

    // matrix_connection.cpp
    extern std::shared_ptr<WebsocketClient> ws_sp;
    extern WebsocketClient *ws;
    extern UpdateChecker::UpdateManager updateManager;
    extern MatrixVersionChecker::MatrixVersionManager matrixVersionManager;
} // namespace desktop_app::detail

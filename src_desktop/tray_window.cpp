#include "tray_window.h"

#include "app_state.h"

#include <hello_imgui/hello_imgui.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace desktop_app::detail {
    volatile sig_atomic_t shouldExit{0};
    std::atomic<bool> showMainWindow{false};
    bool startMinimized = false;
    int startupFrameCounter = 0;

#ifdef _WIN32
    // Console control handler for Windows — must be async-signal-safe
    static BOOL WINAPI console_ctrl_handler(DWORD ctrlType) {
        shouldExit = 1;
        return TRUE;
    }
#else
    static void signal_handler(int /*signum*/) {
        shouldExit = 1;
    }
#endif

    void install_signal_handlers() {
#ifdef _WIN32
        SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
#endif
    }

    // ---- Window iconify callback ----
    void window_iconify_callback(GLFWwindow *window, const int iconified) {
        if (iconified) {
            spdlog::info("Window iconified.");
            HelloImGui::GetRunnerParams()->appWindowParams.hidden = true;
        } else {
            spdlog::info("Window restored.");
            HelloImGui::GetRunnerParams()->appWindowParams.hidden = false;
        }
    }

    // ---- Tray setup ----
    void setup_tray(Tray::Tray &tray) {
        tray.addEntry(
            Tray::Button(
                "Show Window",
                [&] { showMainWindow.store(true); }));
        tray.addEntry(
            Tray::Button(
                "Exit",
                [&] { shouldExit = 1; }));
    }

    void process_window_requests() {
        if (shouldExit) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
            shouldExit = 0;
        }

        if (showMainWindow.load()) {
            spdlog::info("Showing main window.");
            showMainWindow.store(false);
            HelloImGui::GetRunnerParams()->appWindowParams.hidden = false;
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            glfwRestoreWindow(window);
            glfwShowWindow(window);
            glfwFocusWindow(window);
        }
    }
}

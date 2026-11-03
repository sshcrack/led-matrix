#pragma once

#include <GLFW/glfw3.h>
#include <tray.hpp>

namespace desktop_app::detail {
    /// Adds the "Show Window" / "Exit" tray entries.
    void setup_tray(Tray::Tray &tray);

    /// GLFW iconify callback: hides/unhides the HelloImGui window.
    void window_iconify_callback(GLFWwindow *window, int iconified);

    /// Handles pending exit / show-window requests (tray, signals, second instance).
    void process_window_requests();

    /// Installs SIGINT/SIGTERM (or console ctrl) handlers that request exit.
    void install_signal_handlers();
}

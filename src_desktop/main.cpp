// System and standard library includes
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>

// Project includes
#include <shared/common/crash_reporter.h>
#include <shared/common/utils/utils.h>
#include <hello_imgui/hello_imgui.h>
#include <shared/desktop/config.h>
#include <shared/desktop/utils.h>
#include <shared/desktop/glfw.h>
#include "shared/desktop/plugin_loader/loader.h"
#include "single_instance_manager.h"
#include "app_setup.h"
#include "app_state.h"
#include "matrix_connection.h"
#include "settings_panel.h"
#include "tray_window.h"
#include <csignal>

// Third-party includes
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <tray.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace Config;
using namespace desktop_app::detail;

static auto DISPLAY_APP_NAME = "LED Matrix Controller";

// ---- Main application logic ----
int run_app(int argc, char *argv[]) {
    fs::path crash_dir;
    try {
        crash_dir = get_data_dir() / "crashes";
    } catch (...) {
        crash_dir = fs::temp_directory_path() / "led-matrix-desktop-crashes";
    }
    CrashReporter::install({"led-matrix-desktop", crash_dir});
    setup_logging();
    CrashReporter::attach_to_default_logger();
    CrashReporter::set_activity("initializing desktop controller");
    spdlog::info("Crash reports are written to {}", crash_dir.string());

    SingleInstanceManager *instanceManager = nullptr;
    try {
        instanceManager = new SingleInstanceManager("LedMatrixController", [] {
            spdlog::info("Focus request received, showing main window.");
            showMainWindow.store(true);
        });
    } catch ([[maybe_unused]] const std::exception &e) {
        return 0;
    }

    auto pl = Plugins::PluginManager::instance();
    auto cfg = ConfigManager::instance();
    init_plugins();

    // UI state variables (persist via lambda captures)
    SettingsPanel settingsPanel(pl, cfg);

    if (argc > 1 && std::string(argv[1]) == "--start-minimized") {
        spdlog::info("Starting minimized.");
        startMinimized = true;
    }

    const auto trayIco = HelloImGui::AssetFileFullPath("app_settings/icon.ico");
    Tray::Tray tray(DISPLAY_APP_NAME, trayIco);
    setup_tray(tray);

    HelloImGui::RunnerParams runnerParams;
    runnerParams.callbacks.ShowGui = [&] {
        process_window_requests();
        settingsPanel.render();
    };
    runnerParams.callbacks.PreNewFrame = [&] {
        tray.pump();
#ifndef _WIN32
        if (instanceManager)
            instanceManager->poll();
#endif
        for (const auto &[name, plugin]: pl->get_plugins()) {
            plugin->pre_new_frame();
        }
    };
    runnerParams.callbacks.AfterSwap = [&] {
        for (const auto &[name, plugin]: pl->get_plugins()) {
            plugin->after_swap(ImGui::GetCurrentContext());
        }
    };
    runnerParams.callbacks.BeforeExit = [&] {
        spdlog::info("Exiting application...");
        for (auto &[_1, pl]: pl->get_plugins()) {
            pl->before_exit();
        }
    };
    runnerParams.appWindowParams.windowTitle = DISPLAY_APP_NAME;
    runnerParams.imGuiWindowParams.showMenuBar = true;
    runnerParams.iniFilename = (get_data_dir() / "config.ini").string();
    runnerParams.iniFolderType = HelloImGui::IniFolderType::AbsolutePath;
    runnerParams.callbacks.ShowAppMenuItems = [&] {
        render_app_menu_items(runnerParams, cfg);
    };
    runnerParams.fpsIdling.enableIdling = true;
    runnerParams.fpsIdling.fpsIdle = cfg->getGeneralConfig().getFpsLimit();
    runnerParams.fpsIdling.timeActiveAfterLastEvent = 0.0f;
    runnerParams.callbacks.LoadAdditionalFonts = [] {
        load_additional_fonts();
    };
    runnerParams.callbacks.PostInit = [&]() {
        ImGuiMemAllocFunc alloc_fn = nullptr;
        ImGuiMemFreeFunc free_fn = nullptr;
        void *user_data = nullptr;
        ImGui::GetAllocatorFunctions(&alloc_fn, &free_fn, &user_data);
        auto context = ImGui::GetCurrentContext();
        for (auto &[name, plugin]: pl->get_plugins()) {
            plugin->initialize_imgui(context, &alloc_fn, &free_fn, &user_data);
        }
        auto *window = (GLFWwindow *) HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
        setMainGLFWWindow(window);
        glfwSwapInterval(0);
        glfwSetWindowIconifyCallback(window, window_iconify_callback);
        for (auto &[name, plugin]: pl->get_plugins()) {
            plugin->post_init();
        }
        init_websocket();
        updateManager.checkForUpdatesAsync();
    };

    runnerParams.appWindowParams.restorePreviousGeometry = true;

    install_signal_handlers();

    CrashReporter::set_activity("desktop UI event loop");
    HelloImGui::Run(runnerParams);
    CrashReporter::set_activity("shutting down desktop controller");
    spdlog::info("Exiting tray thread...");
    ws->stop();
    tray.exit();
    WebsocketClient::setInstance(nullptr);
    {
        auto generalCfg = cfg->getGeneralConfig();
        auto hostname = generalCfg.getHostnameCopy();
        auto port = generalCfg.getPort();
        if (generalCfg.isTurnMatrixOffOnExit()) {
            spdlog::info("Turning Matrix OFF on exit.");
            change_matrix_status(hostname, port, false, true);
        }
    }

    ws = nullptr;
    ws_sp.reset();
    // Destroy config BEFORE plugins: ~ConfigManager saveConfig()
    // iterates plugins — they must still be alive.
    delete cfg;
    pl->destroy_plugins();
    delete instanceManager;
    spdlog::info("Exited cleanly.");
    return 0;
}

// ---- Platform-specific main entry points ----
#if defined(_WIN32) && !defined(USE_DESKTOP_CONSOLE)
int inner_main(const int argc, char *argv[]) {
    return run_app(argc, argv);
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    LPWSTR fullCmdLine = GetCommandLineW();
    int argc = 0;
    LPWSTR *argv_w = CommandLineToArgvW(fullCmdLine, &argc);
    if (!argv_w)
        return -1;
    char **argv = new char *[argc];
    for (int i = 0; i < argc; ++i) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            for (int j = 0; j < i; ++j)
                delete[] argv[j];
            delete[] argv;
            LocalFree(argv_w);
            return -1;
        }
        argv[i] = new char[len];
        WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], len, nullptr, nullptr);
    }
    int result = -1;
    try {
        result = inner_main(argc, argv);
    } catch (const std::exception &e) {
        CrashReporter::report_exception("desktop startup exception", e.what());
        std::ofstream(get_exec_dir() / "startup-error.log", std::ios::trunc) << e.what() << '\n';
        MessageBoxA(nullptr, e.what(), "LED Matrix Controller startup error", MB_OK | MB_ICONERROR);
    } catch (...) {
        CrashReporter::report_exception("desktop startup exception", "unknown exception");
        std::ofstream(get_exec_dir() / "startup-error.log", std::ios::trunc)
            << "An unknown exception occurred during startup.\n";
        MessageBoxA(nullptr, "An unknown exception occurred during startup.",
                    "LED Matrix Controller startup error", MB_OK | MB_ICONERROR);
    }
    for (int i = 0; i < argc; ++i)
        delete[] argv[i];
    delete[] argv;
    LocalFree(argv_w);
    return result;
}
#else
int main(const int argc, char *argv[]) {
    return run_app(argc, argv);
}
#endif

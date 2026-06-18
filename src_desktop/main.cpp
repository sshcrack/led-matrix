// System and standard library includes
#include <iostream>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <string>

// Project includes
#include "shared/desktop/WebsocketClient.h"
#include <shared/common/utils/utils.h>
#include <hello_imgui/hello_imgui.h>
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "imgui_freetype.h"
#include <shared/desktop/config.h>
#include <shared/desktop/utils.h>
#include <shared/desktop/UpdateManager.h>
#include <shared/desktop/MatrixVersionManager.h>
#include "filters.h"
#include "shared/desktop/plugin_loader/loader.h"
#include "single_instance_manager.h"
#include <shared/desktop/glfw.h>
#include "spdlog/cfg/env.h"
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

// Third-party includes
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tray.hpp>
#include <cpr/cpr.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace Config;

// ---- Platform-specific utility functions ----
#ifndef WIN32
std::string get_noto_color_emoji_path() {
    std::string result;
    const char *cmd = "fc-list :family=file | grep -i 'Noto Color Emoji' | awk -F: '{print $1}' | head -n 1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        return result;
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe.get())) {
        result = buffer;
        // Remove newline
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }
    return result;
}
#endif

// ---- Global/static variables ----
static std::atomic<bool> shouldExit{false};
static auto DISPLAY_APP_NAME = "LED Matrix Controller";
static std::atomic<bool> showMainWindow{false};
// g_pending_http removed — async HTTP calls use detached threads

#ifdef _WIN32
// Console control handler for Windows — must be async-signal-safe
static BOOL WINAPI console_ctrl_handler(DWORD ctrlType) {
    shouldExit.store(true);
    return TRUE;
}
#else
static void signal_handler(int /*signum*/) {
    shouldExit.store(true);
}
#endif

// ---- Window iconify callback ----
static void window_iconify_callback(GLFWwindow *window, const int iconified) {
    if (iconified) {
        spdlog::info("Window iconified.");
        HelloImGui::GetRunnerParams()->appWindowParams.hidden = true;
    } else {
        spdlog::info("Window restored.");
        HelloImGui::GetRunnerParams()->appWindowParams.hidden = false;
    }
}

// ----- Function to change matrix status (synchronous with timeout) ----
static void change_matrix_status(const std::string &hostname, uint16_t port, bool turnOn, bool waitForCompletion = false) {
    auto task = [hostname, port, turnOn]() {
        try {
            auto url = fmt::format("http://{}:{}/set_enabled", hostname, port);

            cpr::Response response = cpr::Post(cpr::Url(url), cpr::Payload{{"enabled", turnOn ? "true" : "false"}}, cpr::Timeout{5000L});
            if (response.error) {
                spdlog::error("Failed to change matrix status: {}", response.error.message);
            } else {
                spdlog::info("Matrix turned {} successfully.", turnOn ? "on" : "off");
            }
        } catch (const std::exception &e) {
            spdlog::error("Exception while changing matrix status: {}", e.what());
        }
    };

    if (waitForCompletion) {
        // Synchronous call for shutdown scenarios
        task();
    } else {
        // Asynchronous call for normal operation (self-contained lambda, safe to detach)
        std::thread(task).detach();
    }
}

// ---- Tray setup ----
static void setup_tray(Tray::Tray &tray) {
    tray.addEntry(
        Tray::Button(
            "Show Window",
            [&] { showMainWindow.store(true); }));
    tray.addEntry(
        Tray::Button(
            "Exit",
            [&] { shouldExit.store(true); }));
}

// ---- File-scope state used by GUI and callbacks ----
static std::shared_ptr<WebsocketClient> ws_sp;
static WebsocketClient *ws = nullptr;
static UpdateChecker::UpdateManager updateManager;
static MatrixVersionChecker::MatrixVersionManager matrixVersionManager;
static bool startMinimized = false;
static int startupFrameCounter = 0;

// ---- Extracted helpers ----
static void setup_logging() {
    fs::path logDir = get_data_dir() / "logs";
    if (!fs::exists(logDir))
        fs::create_directories(logDir);
    auto max_size = 1048576 * 5;
    auto max_files = 3;
    spdlog::cfg::load_env_levels();
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%^%l%$] %v");
    console_sink->set_level(spdlog::level::debug);
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (logDir / "app.log").string(), max_size, max_files);
    file_sink->set_level(spdlog::level::trace);
    spdlog::logger logger("multi_sink", {console_sink, file_sink});
    auto sharedLogger = std::make_shared<spdlog::logger>(logger);
    spdlog::set_default_logger(sharedLogger);
    auto assets_dir = get_exec_dir() / ".." / "share" / "led-matrix-desktop" / "assets";
    if (!fs::is_directory(assets_dir)) {
        assets_dir = get_exec_dir() / ".." / "assets";
    }
    HelloImGui::SetAssetsFolder(assets_dir.string());
}

static void init_plugins() {
    auto pl = Plugins::PluginManager::instance();
    auto cfg = ConfigManager::instance();
    pl->initialize();
    for (const auto &[plName, plugin]: pl->get_plugins()) {
        plugin->load_config(cfg->getPluginSetting(plName));
    }
}



// ---- Main application logic ----
int run_app(int argc, char *argv[]) {
    setup_logging();

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

    // UI state variables (non-static, persist via lambda captures)
    bool initialConnect = true;
    std::string hostname = cfg->getGeneralConfig().getHostname();
    int port = cfg->getGeneralConfig().getPort();
    int fpsLimit = cfg->getGeneralConfig().getFpsLimit();
    int udpFpsLimit = cfg->getGeneralConfig().getUdpFpsLimit();
    bool matrixOnOnStart = cfg->getGeneralConfig().isTurnMatrixOnOnStart();
    bool matrixOffOnExit = cfg->getGeneralConfig().isTurnMatrixOffOnExit();

    if (argc > 1 && std::string(argv[1]) == "--start-minimized") {
        spdlog::info("Starting minimized.");
        startMinimized = true;
    }

    const auto trayIco = HelloImGui::AssetFileFullPath("app_settings/icon.ico");
    Tray::Tray tray(DISPLAY_APP_NAME, trayIco);
    setup_tray(tray);

    HelloImGui::RunnerParams runnerParams;
    runnerParams.callbacks.ShowGui = [&] {
        if (shouldExit.load()) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
            shouldExit.store(false);
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

        General &generalCfg = cfg->getGeneralConfig();
        ImGui::SeparatorText("General Device Settings");

        if (ImGui::InputTextWithHint("LED Matrix hostname", "e.g. 10.4.1.2", &hostname,
                                     ImGuiInputTextFlags_CallbackCharFilter, hostname_filter)) {
            std::cout << "Hostname changed to: " << hostname << std::endl;
            generalCfg.setHostname(hostname);
            ws->stop();
        }

        bool somethingInvalid = false;
        if (hostname.empty()) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::InputScalar("Port", ImGuiDataType_U16, &port, nullptr, nullptr, "%u", ImGuiInputTextFlags_None)) {
            std::cout << "Port changed to: " << port << std::endl;
            generalCfg.setPort(port);
        }

        if (port < 1 || port > 65535) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::InputInt("FPS Limit", &fpsLimit, 1, 5, ImGuiInputFlags_None)) {
            if (fpsLimit < 1) fpsLimit = 1;
            if (fpsLimit > 360) fpsLimit = 360;
            generalCfg.setFpsLimit(fpsLimit);
            HelloImGui::GetRunnerParams()->fpsIdling.fpsIdle = fpsLimit;
        }
        if (fpsLimit < 1 || fpsLimit > 360) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
            somethingInvalid = true;
        }

        if (ImGui::SliderInt("UDP Send FPS", &udpFpsLimit, 1, 120, "%d FPS")) {
            if (udpFpsLimit < 1) udpFpsLimit = 1;
            if (udpFpsLimit > 120) udpFpsLimit = 120;
            generalCfg.setUdpFpsLimit(udpFpsLimit);
        }
        ImGui::SetItemTooltip("Limits the rate at which frames are sent to the Pi. "
                              "Lower this if the Pi can't keep up (e.g. 15-25 FPS).");

        if (somethingInvalid) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Please fix the highlighted fields.");
        } else if (initialConnect) {
            ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
            ws->start();
            matrixVersionManager.checkMatrixVersionAsync(hostname, port);
            initialConnect = false;
            if (generalCfg.isTurnMatrixOnOnStart()) {
                spdlog::info("Turning Matrix ON on start.");
                change_matrix_status(hostname, port, true);
            }
        }

        if (HelloImGui::GetRunnerParams()->appWindowParams.hidden) {
            return;
        }

        if (startMinimized) {
            if (startupFrameCounter > 10) {
                spdlog::info("Hiding window after startup.");
                HelloImGui::GetRunnerParams()->appWindowParams.hidden = true;
                startMinimized = false;
            } else {
                startupFrameCounter++;
            }
            return;
        }

        if (ImGui::Checkbox("Turn Matrix On on Start", &matrixOnOnStart)) {
            generalCfg.setTurnMatrixOnOnStart(matrixOnOnStart);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Turn Matrix Off on Exit", &matrixOffOnExit)) {
            generalCfg.setTurnMatrixOffOnExit(matrixOffOnExit);
        }

        auto state = ws->getReadyState();
        const std::string stateStr = ws->getReadyStateString();
        ImGui::Text("WebSocket is currently: %s", stateStr.c_str());

        if (state != ix::ReadyState::Open) {
            if (ImGui::Button("Connect", ImVec2(0, 0))) {
                ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
                ws->start();
                ws->webSocket.enableAutomaticReconnection();
                matrixVersionManager.checkMatrixVersionAsync(hostname, port);
                spdlog::info("Connecting to WebSocket at ws://{}:{}/desktopWebsocket", hostname, port);
            }
            return;
        } else {
            if (ImGui::Button("Disconnect", ImVec2(0, 0))) {
                ws->stop();
                ws->webSocket.disableAutomaticReconnection();
                spdlog::info("Disconnecting from WebSocket at ws://{}:{}/desktopWebsocket", hostname, port);
            }
        }

        ImGui::Text("Active Scene: %s", ws->getActiveScene().c_str());
        auto last = ws->getLastError();
        if (!last.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Last Error: %s", last.c_str());
        }

        ImGui::SeparatorText("Plugin Settings");
        auto plugins = pl->get_plugins();
        if (plugins.empty()) {
            ImGui::Text("No plugins loaded.");
            return;
        }

        static std::pair<std::string, Plugins::DesktopPlugin *> selected = plugins[0];
        {
            ImGui::BeginChild("Plugin Selector Pane", ImVec2(150, 0),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            for (const auto &currPair : plugins) {
                if (ImGui::Selectable(currPair.first.c_str(), selected.first == currPair.first))
                    selected = currPair;
            }
            ImGui::EndChild();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::BeginChild(selected.first.c_str(), ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        selected.second->render();
        ImGui::EndChild();
        ImGui::EndGroup();

        updateManager.render(ImGui::GetCurrentContext());
        matrixVersionManager.render(ImGui::GetCurrentContext());
        if (updateManager.shallAppExit())
            HelloImGui::GetRunnerParams()->appShallExit = true;
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
        General &generalCfg = cfg->getGeneralConfig();
        bool autostartEnabled = generalCfg.isAutostartEnabled();
        if (ImGui::MenuItem("Start with System", nullptr, autostartEnabled)) {
            generalCfg.setAutostartEnabled(!autostartEnabled);
        }

        if (ImGui::MenuItem("Save Config", nullptr, false)) {
            cfg->saveConfig();
        }

        ImGui::Separator();

        // Matrix version compatibility info
        auto matrixVersionInfo = matrixVersionManager.getLastCheckResult();
        std::string matrixVersionText = "Matrix Version: ";
        ImVec4 matrixVersionColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Default gray

        switch (matrixVersionInfo.compatibility) {
            case MatrixVersionChecker::VersionCompatibility::Compatible:
                matrixVersionText += matrixVersionInfo.version.toString() + " ✓";
                matrixVersionColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
                break;
            case MatrixVersionChecker::VersionCompatibility::MatrixNewer:
                matrixVersionText += matrixVersionInfo.version.toString() + " (newer)";
                matrixVersionColor = ImVec4(0.0f, 0.8f, 1.0f, 1.0f); // Blue
                break;
            case MatrixVersionChecker::VersionCompatibility::DesktopNewer:
                matrixVersionText += matrixVersionInfo.version.toString() + " (needs update)";
                matrixVersionColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Orange
                break;
            case MatrixVersionChecker::VersionCompatibility::NetworkError:
                matrixVersionText += "Connection Error";
                matrixVersionColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                break;
            case MatrixVersionChecker::VersionCompatibility::ParseError:
                matrixVersionText += "Parse Error";
                matrixVersionColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                break;
        }

        // Use colored text for the menu item (read-only)
        ImGui::TextColored(matrixVersionColor, "%s", matrixVersionText.c_str());

        if (matrixVersionInfo.compatibility == MatrixVersionChecker::VersionCompatibility::DesktopNewer) {
            if (ImGui::MenuItem("Show Matrix Update Dialog")) {
                matrixVersionManager.clearDialogs();
                General &generalCfg = cfg->getGeneralConfig();
                matrixVersionManager.checkMatrixVersionAsync(generalCfg.getHostname(), generalCfg.getPort());
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Check for Updates", nullptr, false)) {
            updateManager.checkForUpdatesManual();
        }

        bool updateNotificationsEnabled = updateManager.isUpdateNotificationsEnabled();
        if (ImGui::MenuItem("Update Notifications", nullptr, updateNotificationsEnabled)) {
            updateManager.setUpdateNotificationsEnabled(!updateNotificationsEnabled);
        }

        if (ImGui::BeginMenu("Update Settings")) {
            auto &prefs = updateManager.getUpdatePreferences();

            if (!prefs.skippedVersion.empty()) {
                ImGui::Text("Skipped version: %s", prefs.skippedVersion.c_str());
                if (ImGui::MenuItem("Clear skipped version")) {
                    prefs.skippedVersion = "";
                    prefs.save();
                }
                ImGui::Separator();
            }

            int remindDays = prefs.remindIntervalDays;
            if (ImGui::SliderInt("Remind interval (days)", &remindDays, 1, 30)) {
                prefs.remindIntervalDays = remindDays;
                prefs.save();
            }

            if (ImGui::MenuItem("Reset all preferences")) {
                updateManager.resetUpdatePreferences();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Hide to tray")) {
            runnerParams.appWindowParams.hidden = true;
        }
    };
    runnerParams.fpsIdling.enableIdling = true;
    runnerParams.fpsIdling.fpsIdle = cfg->getGeneralConfig().getFpsLimit();
    runnerParams.fpsIdling.timeActiveAfterLastEvent = 0.0f;
    runnerParams.callbacks.LoadAdditionalFonts = [] {
        const ImGuiIO &io = ImGui::GetIO();
#ifdef WIN32
        std::string fontPath = "C:/Windows/Fonts/seguiemj.ttf";
#else
        std::string fontPath = get_noto_color_emoji_path();
#endif
        HelloImGui::ImGuiDefaultSettings::LoadDefaultFont_WithFontAwesomeIcons();
        if (!fontPath.empty()) {
            static ImFontConfig fontCfg;
            fontCfg.MergeMode = true;
            fontCfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 0.0f, &fontCfg);
            io.Fonts->Build();
        }
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
        ws_sp = std::make_shared<WebsocketClient>();
        ws = ws_sp.get();
        WebsocketClient::setInstance(ws);
        ws_sp->setup_callback();
        updateManager.checkForUpdatesAsync();
    };

    runnerParams.appWindowParams.restorePreviousGeometry = true;

#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif

    HelloImGui::Run(runnerParams);
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
    pl->destroy_plugins();
    delete cfg;
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
    const int result = inner_main(argc, argv);
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

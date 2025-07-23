#include <tray.hpp>
#include <iostream>
#include <fmt/format.h>
#include <shared/common/utils/utils.h>
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include <hello_imgui/hello_imgui.h>
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "WebsocketClient.h"
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <shared/desktop/config.h>
#include <shared/desktop/utils.h>
#include "filters.h"

#include "shared/desktop/plugin_loader/loader.h"
#include "single_instance_manager.h"
#include <filesystem>
#ifdef _WIN32
#include <windows->h>
#endif
#include "spdlog/cfg/env.h"

namespace fs = std::filesystem;
static bool shouldExit = false;
static auto DISPLAY_APP_NAME = "LED Matrix Controller";

static void window_iconify_callback(GLFWwindow *window, const int iconified)
{
    if (iconified)
    {
        spdlog::info("Window iconified.");
        HelloImGui::GetRunnerParams()->appWindowParams.hidden = true;
    }
    else
    {
        spdlog::info("Window restored.");
        HelloImGui::GetRunnerParams()->appWindowParams.hidden = false;
    }
}

static bool showMainWindow = false;
using namespace Config;

#if defined(_WIN32) && !defined(USE_DESKTOP_CONSOLE)
int inner_main(const int argc, char *argv[])
#else
int main(const int argc, char *argv[])
#endif
{
    fs::path logDir = get_data_dir() / "logs";
    if (!fs::exists(logDir))
        fs::create_directories(logDir);

    // Create a file rotating logger with 5 MB size max and 3 rotated files
    auto max_size = 1048576 * 5;
    auto max_files = 3;

    spdlog::cfg::load_env_levels();
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>((logDir / "app.log").string(), max_size, max_files);
    file_sink->set_level(spdlog::level::trace);

    spdlog::logger logger("multi_sink", {console_sink, file_sink});
    auto sharedLogger = std::make_shared<spdlog::logger>(logger);

    spdlog::set_default_logger(sharedLogger);
    HelloImGui::SetAssetsFolder((get_exec_dir() / ".." / "assets").string());

    // Single instance manager
    SingleInstanceManager *instanceManager = nullptr;
    try
    {
        instanceManager = new SingleInstanceManager("LedMatrixController", []
                                                    { showMainWindow = true; });
    }
    catch ([[maybe_unused]] const std::exception &e)
    {
        // Already running, exit
        return 0;
    }

    auto pl = Plugins::PluginManager::instance();
    auto cfg = ConfigManager::instance();
    pl->initialize();

    for (const auto &[plName, plugin] : pl->get_plugins())
    {
        plugin->load_config(cfg->getPluginSetting(plName));
    }

    static WebsocketClient* ws = new WebsocketClient();
    auto guiFunction = [pl, cfg] {
        static bool initialConnect = true;
        if (shouldExit)
        {
            HelloImGui::GetRunnerParams()->appShallExit = true;
            shouldExit = false;
        }

        if (showMainWindow)
        {
            spdlog::info("Showing main window.");
            showMainWindow = false;
            HelloImGui::GetRunnerParams()->appWindowParams.hidden = false;
        }

        General &generalCfg = cfg->getGeneralConfig();
        ImGui::SeparatorText("General Device Settings");

        static std::string hostname = generalCfg.getHostname();
        if (ImGui::InputTextWithHint("LED Matrix hostname", "e.g. 10.4.1.2", &hostname, ImGuiInputTextFlags_CallbackCharFilter, HostnameFilter))
        {
            std::cout << "Hostname changed to: " << hostname << std::endl;
            generalCfg.setHostname(hostname);
            ws->stop();
        }

        bool somethingInvalid = false;
        if (hostname.empty())
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f); // 2.0f = thickness
            somethingInvalid = true;
        }

        static int port = generalCfg.getPort();
        if (ImGui::InputScalar("Port", ImGuiDataType_U16, &port, nullptr, nullptr, "%u", ImGuiInputTextFlags_None))
        {
            std::cout << "Port changed to: " << port << std::endl;
            generalCfg.setPort(port);
        }

        if (port < 1 || port > 65535)
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f); // 2.0f = thickness
            somethingInvalid = true;
        }

        if (somethingInvalid)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Please fix the highlighted fields.");
        } else if (initialConnect) {
            ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
            ws->start();
            initialConnect = false;
        }

        auto state = ws->getReadyState();
        if (state != ix::ReadyState::Open)
        {
            const std::string stateStr = ws->getReadyStateString();
            std::string statusText = "WebSocket is currently: " + stateStr;

            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), statusText.c_str());
            if (ImGui::Button("Connect", ImVec2(0, 0)))
            {
                ws->setUrl(fmt::format("ws://{}:{}/desktopWebsocket", hostname, port));
                ws->start();

                spdlog::info("Connecting to WebSocket at ws://{}:{}/desktopWebsocket", hostname, port);
            }

            return;
        }

        ImGui::Text(("Active Scene: " + ws->getActiveScene()).c_str());

        ImGui::SeparatorText("Plugin Settings");
        auto plugins = pl->get_plugins();
        if (plugins.empty())
        {
            ImGui::Text("No plugins loaded.");
            return;
        }

        static std::pair<std::string, Plugins::DesktopPlugin *> selected = plugins[0];
        {
            ImGui::BeginChild("Plugin Selector Pane", ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            for (const auto &currPair : plugins)
            {
                std::string currName = currPair.first;
                if (ImGui::Selectable(currName.c_str(), selected.first == currName))
                    selected = currPair;
            }

            ImGui::EndChild();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::BeginChild(selected.first.c_str(), ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

        selected.second->render(ImGui::GetCurrentContext());

        ImGui::EndChild();
        ImGui::EndGroup();
    };

    const auto trayIco = HelloImGui::AssetFileFullPath("app_settings/icon.ico");
    Tray::Tray tray(DISPLAY_APP_NAME, trayIco);
    tray.addEntry(
        Tray::Button(
            "Show Window",
            [&]
            {
                showMainWindow = true;
            }));
    tray.addEntry(
        Tray::Button(
            "Exit",
            [&]
            {
                shouldExit = true;
            }));

    HelloImGui::RunnerParams runnerParams;
    runnerParams.callbacks.ShowGui = guiFunction;
    runnerParams.callbacks.PreNewFrame = [&]
    {
        tray.pump();
    };

    runnerParams.callbacks.BeforeExit = [&]
    {
        spdlog::info("Exiting application...");
        for (auto &[_1, pl] : pl->get_plugins())
        {
            pl->before_exit();
        }
    };
    runnerParams.appWindowParams.windowTitle = DISPLAY_APP_NAME;
    runnerParams.imGuiWindowParams.showMenuBar = true;

    runnerParams.callbacks.ShowAppMenuItems = [&]
    {
        General &generalCfg = cfg->getGeneralConfig();
        bool autostartEnabled = generalCfg.isAutostartEnabled();
        if (ImGui::MenuItem("Start with System", nullptr, autostartEnabled))
        {
            generalCfg.setAutostartEnabled(!autostartEnabled);
        }

        if (ImGui::MenuItem("Save Config", nullptr, false))
        {
            cfg->saveConfig();
        }
    };

    runnerParams.callbacks.PostInit = [&]()
    {
        auto *window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
        glfwSetWindowIconifyCallback(window, window_iconify_callback);
    };

    runnerParams.appWindowParams.restorePreviousGeometry = true;
    if (argc > 1 && std::string(argv[1]) == "--start-minimized")
    {
        runnerParams.appWindowParams.hidden = true;
    }

    // Add polling for DBus messages (Linux only)
#ifndef _WIN32
    runnerParams.callbacks.PreNewFrame = [&]()
    {
        if (instanceManager)
            instanceManager->poll();
    };
#endif

    HelloImGui::Run(runnerParams);
    spdlog::info("Exiting tray thread...");
    tray.exit(); // Ensure tray.exit() is called after HelloImGui::Run
    delete ws;

    delete cfg;
    pl->destroy_plugins();
    delete instanceManager;

    spdlog::info("Exited cleanly.");
    return 0;
}

#if defined(_WIN32) && !defined(USE_DESKTOP_CONSOLE)
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    int argc = 0;
    LPWSTR *argv_w = CommandLineToArgvW(pCmdLine, &argc);
    if (!argv_w)
        return -1;

    // Convert wide strings to UTF-8
    char **argv = new char *[argc];
    for (int i = 0; i < argc; ++i)
    {
        const int len = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i] = new char[len];
        WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], len, nullptr, nullptr);
    }

    const int result = inner_main(argc, argv); // Call your real main

    // Cleanup
    for (int i = 0; i < argc; ++i)
    {
        delete[] argv[i];
    }
    delete[] argv;
    LocalFree(argv_w);

    return result;
}
#endif
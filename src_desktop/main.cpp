#include <tray.hpp>
#include <iostream>
#include <fmt/format.h>
#include <shared/common/utils/utils.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <GLFW/glfw3.h>
#include <hello_imgui/hello_imgui.h>
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include <thread>
#include <spdlog/spdlog.h>
#include "shared/desktop/config.h"
#include "filters.h"

#include "shared/desktop/plugin_loader/loader.h"
#include "single_instance_manager.h"

static bool shouldExit = false;
static const char *DISPLAY_APP_NAME = "LED Matrix Controller";

static void window_iconify_callback(GLFWwindow *window, int iconified)
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
int main(int argc, char *argv[])
{
    HelloImGui::SetAssetsFolder((get_exec_dir() / ".." / "assets").string());

    // Single instance manager
    SingleInstanceManager* instanceManager = nullptr;
    try {
        instanceManager = new SingleInstanceManager("LedMatrixController", [](){
            showMainWindow = true;
        });
    } catch (const std::exception& e) {
        // Already running, exit
        return 0;
    }

    auto pl = Plugins::PluginManager::instance();
    auto cfg = ConfigManager::instance();
    pl->initialize();

    for (auto const plugin : pl->get_plugins())
    {
        plugin.second->loadConfig(cfg->getPluginSetting(plugin.first));
    }

    auto guiFunction = [pl, cfg]()
    {
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

        ImGui::Text("LED Matrix (required)", "");
        ImGui::SameLine();

        static std::string hostname = generalCfg.getHostname();
        if (ImGui::InputTextWithHint("##HostnamePort", "hostname:port", &hostname, ImGuiInputTextFlags_CallbackCharFilter, HostPortFilter))
        {
            generalCfg.setHostnameAndPort(hostname);
        }

        if (hostname.empty())
        {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            draw_list->AddRect(min, max, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f); // 2.0f = thickness
        }

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
            for (const auto currPair : plugins)
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

    Tray::Tray tray(DISPLAY_APP_NAME, "icon.ico");
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

    std::thread trayThread(
        [&tray]()
        {
            tray.run();
        });

    HelloImGui::RunnerParams runnerParams;
    runnerParams.callbacks.ShowGui = guiFunction;
    runnerParams.callbacks.BeforeExit = [&]()
    {
        spdlog::info("Exiting application...");
        for (auto && plugin : pl->get_plugins()) {
            plugin.second->beforeExit();
        }
    };
    runnerParams.appWindowParams.windowTitle = DISPLAY_APP_NAME;
    runnerParams.imGuiWindowParams.showMenuBar = true;

    runnerParams.callbacks.ShowAppMenuItems = [&]()
    {
        General &generalCfg = cfg->getGeneralConfig();
        bool autostartEnabled = generalCfg.isAutostartEnabled();
        if (ImGui::MenuItem("Start with System", NULL, autostartEnabled))
        {
            generalCfg.setAutostartEnabled(!autostartEnabled);
        }

        if (ImGui::MenuItem("Save Config", NULL, false))
        {
            cfg->saveConfig();
        }
    };

    runnerParams.callbacks.PostInit = [&]()
    {
        GLFWwindow *window = (GLFWwindow *) HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
        glfwSetWindowIconifyCallback(window, window_iconify_callback);
    };

    runnerParams.appWindowParams.restorePreviousGeometry = true;
    if (argc > 1 && std::string(argv[1]) == "--start-minimized")
    {
        runnerParams.appWindowParams.hidden = true;
    }

    // Add polling for DBus messages (Linux only)
#ifndef _WIN32
    runnerParams.callbacks.PreNewFrame = [&]() {
        if (instanceManager) instanceManager->poll();
    };
#endif

    HelloImGui::Run(runnerParams);
    spdlog::info("Exiting tray thread...");
    tray.exit(); // Ensure tray.exit() is called after HelloImGui::Run
    spdlog::info("Joining tray thread...");
#ifndef _WIN32
    trayThread.join();
#endif
    delete cfg;
    pl->destroy_plugins();
    delete instanceManager;
    return 0;
}

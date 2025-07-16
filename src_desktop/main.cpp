#include <tray.hpp>
#include <iostream>
#include <fmt/format.h>
#include <shared/common/utils/utils.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <hello_imgui/hello_imgui.h>
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "toolbar.h"
#include <thread>
#include <spdlog/spdlog.h>
#include "shared/desktop/config.h"
#include "filters.h"

#include "shared/desktop/plugin_loader/loader.h"

static bool showWindowClicked = false;
static bool shouldExit = false;
static bool hasStartedMinimized = false;

using namespace Config;
int main(int argc, char *argv[])
{
    HelloImGui::SetAssetsFolder(get_exec_dir() / ".." / "assets");

    auto pl = Plugins::PluginManager::instance();
        auto cfg = ConfigManager::instance();
    pl->initialize();

    auto guiFunction = [pl, cfg]()
    {
        bool autostart;
        if (ImGui::Checkbox("Autostart", &autostart))
        {
            cfg->getGeneralConfig().setAutostartEnabled(autostart);
        }

        ImGui::SameLine();

        if (ImGui::Button("Minimize to Toolbar"))
        {
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            minimizeToTray(window);
        }

        ImGui::SeparatorText("General Device Settings");

        ImGui::Text("LED Matrix (required)", "");
        ImGui::SameLine();

        General &generalCfg = cfg->getGeneralConfig();
        static std::string hostname = generalCfg.getHostname();
        if (ImGui::InputTextWithHint("", "hostname:port", &hostname, ImGuiInputTextFlags_CallbackCharFilter, HostPortFilter))
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
        const auto ctx = ImGui::GetCurrentContext();
        for (const auto &[name, plugin] : pl->get_plugins())
        {
            if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                plugin->render(ctx);
            }
        }

        if (showWindowClicked)
        {
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            restoreFromTray(window);
            showWindowClicked = false;
        }

        if (shouldExit)
        {
            HelloImGui::GetRunnerParams()->appShallExit = true;
            shouldExit = false;
        }
    };

    Tray::Tray tray("LED Matrix Controller", "icon.ico");
    tray.addEntry(Tray::Button("Show Window", [&]
                               { showWindowClicked = true; }));
    tray.addEntry(Tray::Button("Exit", [&]
                               { shouldExit = true; }));

    std::thread trayThread([&tray]()
                           { tray.run(); });

    HelloImGui::RunnerParams runnerParams;
    runnerParams.callbacks.ShowGui = guiFunction;
    runnerParams.appWindowParams.windowTitle = "LED Matrix Controller";
    runnerParams.appWindowParams.restorePreviousGeometry = true;
    runnerParams.callbacks.EnqueuePostInit([]()
                                           { minimizeToTray((GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow); });

    if (argc > 1 && std::string(argv[1]) == "--start-minimized")
    {
        runnerParams.appWindowParams.hidden = true;
    }

    HelloImGui::Run(runnerParams);
    tray.exit(); // Ensure tray.exit() is called after HelloImGui::Run
    trayThread.join();

    pl->destroy_plugins();
    delete cfg;

    return 0;
}

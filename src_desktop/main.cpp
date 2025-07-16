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
#include "autostart.h"
#include "shared/desktop/config.h"

#include "shared/desktop/plugin_loader/loader.h"


static bool showWindowClicked = false;
static bool shouldExit = false;
static bool hasStartedMinimized = false;

using namespace Config;
int main(int argc, char *argv[])
{
    HelloImGui::SetAssetsFolder(get_exec_dir().value_or(".") + "/../assets");

    auto instance = Plugins::PluginManager::instance();
    instance->initialize();

    auto guiFunction = [instance]()
    {
        auto instance = ConfigManager::instance();
        bool autostart;
        if (ImGui::Checkbox("Autostart", &autostart))
        {
            instance.getGeneralConfig().setAutostartEnabled(autostart);
        }

        ImGui::SameLine();

        if (ImGui::Button("Minimize to Toolbar"))
        {
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            minimizeToTray(window);
        }

        ImGui::SeparatorText("General Device Settings");

        ImGui::Text("Hostname", "");
        ImGui::SameLine();

        static std::string hostname;
        if(ImGui::InputTextWithHint("", "LED Matrix Hostname", &hostname)) {
            Config::General config;
            config.setHostname(hostname);
        }

        ImGui::SeparatorText("Plugin Settings");
        const auto ctx = ImGui::GetCurrentContext();
        for (const auto &[name, plugin] : instance->get_plugins())
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

    return 0;
}

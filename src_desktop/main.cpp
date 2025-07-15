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

#include "shared/desktop/plugin_loader/loader.h"

static bool showWindowClicked = false;
static bool shouldExit = false;

int main(int argc, char *argv[])
{
    HelloImGui::SetAssetsFolder(get_exec_dir().value_or(".") + "/../assets");

    auto instance = Plugins::PluginManager::instance();
    instance->initialize();

    auto guiFunction = [instance]()
    {
        static std::string hostname;
        ImGui::InputTextWithHint("Hostname", "LED Matrix Hostname", &hostname);

        const auto ctx = ImGui::GetCurrentContext();
        for (const auto &[name, plugin] : instance->get_plugins())
        {
            if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                plugin->render(ctx);
            }
        }

        if (ImGui::Button("Minimize to Toolbar"))
        {
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            minimizeToTray(window);
        }

        if (showWindowClicked)
        {
            auto window = (GLFWwindow *)HelloImGui::GetRunnerParams()->backendPointers.glfwWindow;
            restoreFromTray(window);
            showWindowClicked = false;
        }

        if(shouldExit) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
            shouldExit = false;
        }
    };

    Tray::Tray tray("LED Matrix Controller", "icon.ico");
    tray.addEntry(Tray::Button("Show Window", [&] { showWindowClicked = true; }));
    tray.addEntry(Tray::Button("Exit", [&] { shouldExit = true; }));

    std::thread trayThread([&tray]() {
        tray.run();
    });

    HelloImGui::Run(guiFunction, "LED Matrix Controller", false, true);
    tray.exit(); // Ensure tray.exit() is called after HelloImGui::Run
    trayThread.join();

    return 0;
}

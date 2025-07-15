#include <iostream>
#include <fmt/format.h>
#include <shared/common/utils/utils.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <hello_imgui/hello_imgui.h>
#include "imgui_impl_glfw.h"
#include "toolbar.h"

#include "shared/desktop/plugin_loader/loader.h"

int main(int argc, char *argv[])
{
    HelloImGui::SetAssetsFolder(get_exec_dir().value_or(".") + "/../assets");

    auto instance = Plugins::PluginManager::instance();
    instance->initialize();

    auto guiFunction = [instance]()
    {
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
            minimizeToToolbar(window);
        }
    };

    HelloImGui::Run(guiFunction, "LED Matrix Controller", true, true);
    return 0;
}

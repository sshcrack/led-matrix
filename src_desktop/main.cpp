#include "GL/glew.h"
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>
#include <fmt/format.h>
#include <shared/common/utils/utils.h>
#include <nlohmann/json.hpp>
#include <fstream>

#include "shared/desktop/plugin_loader/loader.h"

using std::cout;
using std::endl;


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#include "initialization.h"
#include "render_loop.h"
#include "cleanup.h"

int main(int argc, char *argv[]) {
    const char* glsl_version = nullptr;
    initializeGLFW(glsl_version);

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = createWindow(main_scale);

    setupImGui(window, glsl_version, main_scale);

    auto instance = Plugins::PluginManager::instance();
    instance->initialize();

    renderLoop(window, instance, ImGui::GetIO().Fonts->Fonts[0]);

    cleanup(window);
    return 0;
}

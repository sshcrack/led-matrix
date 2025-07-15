#include "GL/glew.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <fmt/format.h>
#include "shared/desktop/plugin_loader/loader.h"
#include "toolbar.h"

void renderLoop(GLFWwindow* window, Plugins::PluginManager* instance, ImFont* font) {
    constexpr auto clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::PushFont(font);

        ImGui::Text(fmt::format("Loaded {} plugins", instance->get_plugins().size()).c_str());

        const auto ctx = ImGui::GetCurrentContext();
        for (const auto& [name, plugin] : instance->get_plugins()) {
            if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                plugin->render(ctx);
            }
        }

        if (ImGui::Button("Minimize to Toolbar")) {
            minimizeToToolbar(window);
        }

        ImGui::PopFont();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

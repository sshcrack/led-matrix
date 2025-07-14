//
// Created by Hendrik on 14.07.2025.
//
#include <iostream>
#include "hello_imgui/hello_imgui.h"
#include "shared/desktop/plugin_loader/loader.h"

using std::cout;
using std::endl;
int main(int argc, char *argv[])
{
    auto instance = Plugins::PluginManager::instance();
    instance->initialize();
    cout << "Hello, ImGui!" << endl;
    cout << "Loaded " << instance->get_plugins().size() << " plugins." << endl;

    auto guiFunction = []() {
        ImGui::Text("Hello, ");                    // Display a simple label
        HelloImGui::ImageFromAsset("world.jpg");   // Display a static image
        if (ImGui::Button("Bye!"))                 // Display a button
            // and immediately handle its action if it is clicked!
            HelloImGui::GetRunnerParams()->appShallExit = true;
     };
    HelloImGui::Run(guiFunction, "LED Matrix Controller", true);
    return 0;
}
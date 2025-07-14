//
// Created by Hendrik on 14.07.2025.
//
#include <iostream>
#include "hello_imgui/hello_imgui.h"

using std::cout;
using std::endl;
int main(int argc, char *argv[])
{
    cout << "Hello, world!" << endl;
    auto guiFunction = []() {
        ImGui::Text("Hello, ");                    // Display a simple label
        HelloImGui::ImageFromAsset("world.jpg");   // Display a static image
        if (ImGui::Button("Bye!"))                 // Display a button
            // and immediately handle its action if it is clicked!
            HelloImGui::GetRunnerParams()->appShallExit = true;
     };
    HelloImGui::Run(guiFunction, "Hello, globe", true);
    return 0;
}
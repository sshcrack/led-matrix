#include "AudioVisualizerDesktop.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "imgui_stdlib.h"

extern "C" PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer()
{
    return new AudioVisualizerDesktop();
}

extern "C" PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c)
{
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() = default;

AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

int PortFilter(ImGuiInputTextCallbackData *data)
{
    if (data->EventChar < 32 || data->EventChar >= 127)
        return 1; // Disallow non-printable

    if (data->BufTextLen > 5)
    {
        return 1; // Limit to 5 characters
    }

    char c = static_cast<char>(data->EventChar);
    if (c >= '0' && c <= '9')
    {
        return 0; // Allow
    }

    return 1; // Block everything else
}

void AudioVisualizerDesktop::render(ImGuiContext *ctx)
{
    ImGui::SetCurrentContext(ctx);

    ImGui::BeginChild("AudioVisualizer");
    static std::string port = std::to_string(pluginConfig.port);
    if (ImGui::InputText("Port", &port, ImGuiInputTextFlags_CallbackCharFilter, PortFilter))
    {
        try
        {
            pluginConfig.port = std::stoi(port);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid port number: " << e.what() << std::endl;
        }
    }

    ImGui::EndChild();
}

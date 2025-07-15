#include "AudioVisualizerDesktop.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

extern "C" [[maybe_unused]] PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer()
{
    return new AudioVisualizerDesktop();
}

extern "C" [[maybe_unused]] PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c)
{
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() = default;

AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

void AudioVisualizerDesktop::render(ImGuiContext *ctx)
{
    ImGui::SetCurrentContext(ctx);
}

void AudioVisualizerDesktop::loadConfig(const nlohmann::json &config)
{
    autostart = config.value("autostart", false);
    minimizeToToolbar = config.value("minimizeToToolbar", false);
}

void AudioVisualizerDesktop::saveConfig(nlohmann::json &config) const
{
    config = pluginConfig;
}

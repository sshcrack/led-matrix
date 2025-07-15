#include "AudioVisualizerDesktop.h"
#include <iostream>

extern "C" [[maybe_unused]] PLUGIN_EXPORT AudioVisualizerDesktop *createAudioVisualizer() {
    return new AudioVisualizerDesktop();
}

extern "C" [[maybe_unused]] PLUGIN_EXPORT void destroyAudioVisualizer(AudioVisualizerDesktop *c) {
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() {
    std::cout << "AudioVisualizerDesktop plugin initialized" << std::endl;
}

AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

void AudioVisualizerDesktop::render(ImGuiContext* ctx) {
    ImGui::SetCurrentContext(ctx);

    ImGui::Begin("AudioVisualizer");
    ImGui::Text("Audio Visualizer Plugin for Desktop");
    ImGui::Text("This is a placeholder for the audio visualizer functionality.");
    ImGui::End();
}

#include "AudioVisualizerDesktop.h"
#include <iostream>

extern "C" [[maybe_unused]] AudioVisualizerDesktop *createAudioVisualizer()
{
    return new AudioVisualizerDesktop();
}

extern "C" [[maybe_unused]] void destroyAudioVisualizer(AudioVisualizerDesktop *c)
{
    delete c;
}

AudioVisualizerDesktop::AudioVisualizerDesktop() {
    std::cout << "AudioVisualizerDesktop plugin initialized" << std::endl;
}

AudioVisualizerDesktop::~AudioVisualizerDesktop() = default;

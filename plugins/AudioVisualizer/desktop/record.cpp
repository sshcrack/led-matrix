#include "record.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <portaudio.h>
#include <iostream>

namespace AudioRecorder {
Recorder::Recorder() : recording(false), currentDeviceIndex(-1) {
}

Recorder::~Recorder() {
    if (recording) stopRecording();
}

std::vector<Recorder::DeviceInfo> Recorder::listDevices() {
    std::vector<DeviceInfo> devices;
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0) {
            devices.push_back({i, info->name});
        }
    }
    return devices;
}

bool Recorder::startRecording(int deviceIndex) {
    if (recording) return false;
    // ...setup PortAudio stream for deviceIndex...
    // Example skeleton:
    // PaStreamParameters inputParams;
    // inputParams.device = deviceIndex;
    // inputParams.channelCount = 1; // mono
    // inputParams.sampleFormat = paInt16;
    // inputParams.suggestedLatency = Pa_GetDeviceInfo(deviceIndex)->defaultLowInputLatency;
    // inputParams.hostApiSpecificStreamInfo = nullptr;
    // Pa_OpenStream(...);
    // Pa_StartStream(...);
    recording = true;
    currentDeviceIndex = deviceIndex;
    return true;
}

void Recorder::stopRecording() {
    if (!recording) return;
    // ...stop and close PortAudio stream...
    recording = false;
    currentDeviceIndex = -1;
}

bool Recorder::isRecording() const {
    return recording;
}
}

#pragma once
#include <vector>
#include <string>

namespace AudioRecorder {
class Recorder {
public:
    struct DeviceInfo {
        int index;
        std::string name;
    };

    Recorder();
    ~Recorder();

    // List all output devices
    std::vector<DeviceInfo> listDevices();

    // Start recording from selected device
    bool startRecording(int deviceIndex);

    // Stop recording
    void stopRecording();

    // Is currently recording
    bool isRecording() const;

private:
    bool recording;
    int currentDeviceIndex;
    // ... PortAudio stream and other members ...
};
}
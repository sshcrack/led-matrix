#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <deque>
#include <portaudio.h>

namespace AudioRecorder
{
    class Recorder
    {
    public:
        struct DeviceInfo
        {
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

        // Get the latest audio samples
        std::vector<float> getLatestSamples(size_t numSamples);

        // Get the current sample rate
        double getSampleRate() const;

    private:
        std::atomic<bool> recording;
        int currentDeviceIndex;
        PaStream *stream;
        std::deque<float> audioBuffer;
        std::mutex bufferMutex;
        double sampleRate;
        static constexpr size_t MAX_BUFFER_SIZE = 8192; // Maximum buffer size to prevent memory issues

        static int audioCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData);
    };
}
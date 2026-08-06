#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <optional>
#include <deque>
#include <thread>
#include <portaudio.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

static constexpr size_t BUFFER_SIZE = 2048;
static constexpr size_t FFT_SIZE = 1024;

// Sentinel device name for "follow the default output device" loopback mode
static const std::string DEFAULT_LOOPBACK_DEVICE_NAME = "Default Output Device (Loopback)";

namespace AudioRecorder
{
    class Recorder
    {
    public:
        struct DeviceInfo
        {
            int index;
            std::string name;
            bool isLoopback = false;
        };

        Recorder();
        ~Recorder();

        // List all output devices
        static std::vector<DeviceInfo> listDevices();

        // Start recording from selected PortAudio input device
        bool startRecording(int deviceIndex);

        // Capture the current desktop output. On Windows this resolves a WASAPI
        // loopback device; on Linux this records PipeWire/PulseAudio's default
        // sink monitor through parec.
        bool startDefaultOutputLoopback();
        static bool isDefaultOutputLoopbackAvailable();

        // Find the loopback device index corresponding to the current default output device.
        // Returns -1 if not found or not on Windows/WASAPI.
        static int getDefaultOutputLoopbackIndex();

        // Stop recording
        void stopRecording();

        // Is currently recording
        bool isRecording() const;

        // Get the current sample rate
        double getSampleRate() const;

        // Get the current device index being recorded from (-1 if not recording)
        int getCurrentDeviceIndex() const { return currentDeviceIndex; }

        std::optional<std::vector<float>> getLastSamples();

    private:
        std::atomic<bool> recording;
        int currentDeviceIndex;
        PaStream *stream;

#ifndef _WIN32
        int loopbackPipeFd = -1;
        pid_t loopbackPid = -1;
        std::thread loopbackThread;
        std::atomic<bool> stopLoopbackThread{false};

        void linuxLoopbackReadLoop();
#endif

        std::mutex audioBufferMutex;
        std::deque<float> audioBuffer;

        double sampleRate;
        static constexpr size_t MAX_BUFFER_SIZE = 2048; // Maximum buffer size to prevent memory issues

        static int audioCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData);
    };
}

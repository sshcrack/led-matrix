#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <portaudio.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

// The music analyzer uses a long window for bass resolution, but advances in
// small hops. Keeping a rolling window avoids the old bursty behaviour where
// stale chunks were replayed after the UI/render thread fell behind.
static constexpr size_t FFT_SIZE = 1024;
static constexpr size_t MUSIC_ANALYSIS_WINDOW_SIZE = 4096;
static constexpr size_t FFT_HOP_SIZE = 256;

// Sentinel device name for "follow the default output device" loopback mode.
static const std::string DEFAULT_LOOPBACK_DEVICE_NAME = "Default Output Device (Loopback)";

namespace AudioRecorder
{
    struct CapturedAudioFrame
    {
        std::vector<float> mono;
        std::vector<float> left;
        std::vector<float> right;
        double sampleRate = 44100.0;
        uint64_t sequence = 0;

        [[nodiscard]] bool stereo() const
        {
            return !left.empty() && left.size() == right.size();
        }
    };

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

        static std::vector<DeviceInfo> listDevices();
        bool startRecording(int deviceIndex);
        bool startDefaultOutputLoopback();
        static bool isDefaultOutputLoopbackAvailable();
        static int getDefaultOutputLoopbackIndex();
        void stopRecording();
        [[nodiscard]] bool isRecording() const;
        [[nodiscard]] double getSampleRate() const;
        [[nodiscard]] int getCurrentDeviceIndex() const { return currentDeviceIndex; }
        [[nodiscard]] size_t getBufferedFrameCount() const;
        [[nodiscard]] double getBufferedLatencyMs() const;

        // Returns the newest rolling analysis window after at least one hop of
        // fresh audio has arrived. No queued/stale audio is replayed.
        std::optional<CapturedAudioFrame> getLastSamples();

    private:
        struct StereoFrame
        {
            float left = 0.0f;
            float right = 0.0f;
        };

        std::atomic<bool> recording;
        int currentDeviceIndex;
        PaStream *stream;
        int channelCount = 1;

#ifndef _WIN32
        int loopbackPipeFd = -1;
        pid_t loopbackPid = -1;
        std::thread loopbackThread;
        std::atomic<bool> stopLoopbackThread{false};
        void linuxLoopbackReadLoop();
#endif

        mutable std::mutex audioBufferMutex;
        std::deque<StereoFrame> audioBuffer;
        uint64_t capturedFrameSequence = 0;
        uint64_t lastDeliveredSequence = 0;

        double sampleRate;
        static constexpr size_t MAX_BUFFER_FRAMES = MUSIC_ANALYSIS_WINDOW_SIZE * 3;

        static int audioCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData);
    };
}

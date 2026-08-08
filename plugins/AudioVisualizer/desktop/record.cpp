#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "record.h"

#ifdef _WIN32
#include <windows.h>

#ifdef PA_USE_WASAPI
#include <pa_win_wasapi.h>
#endif

#endif

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <filesystem>
#include <spdlog/spdlog.h>


#ifndef _WIN32
namespace
{
    bool executableOnPath(const char *name)
    {
        const char *path = std::getenv("PATH");
        if (!path || !*path)
            return false;

        std::string paths(path);
        size_t begin = 0;
        while (begin <= paths.size())
        {
            const size_t end = paths.find(':', begin);
            const std::string dir = paths.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
            const std::filesystem::path candidate = (dir.empty() ? std::filesystem::path(".") : std::filesystem::path(dir)) / name;
            if (::access(candidate.c_str(), X_OK) == 0)
                return true;
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        return false;
    }
}
#endif

namespace AudioRecorder
{
    Recorder::Recorder() : recording(false), currentDeviceIndex(-1), stream(nullptr), sampleRate(44100.0)
    {
        Pa_Initialize();
    }

    Recorder::~Recorder()
    {
        if (recording)
            stopRecording();

        Pa_Terminate();
    }

    std::vector<Recorder::DeviceInfo> Recorder::listDevices()
    {
        std::vector<DeviceInfo> devices;
        const int numDevices = Pa_GetDeviceCount();
#ifdef _WIN32
        int hostApi = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
#else
        int hostApi = Pa_GetDefaultHostApi();
#endif

        for (int i = 0; i < numDevices; ++i)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (!info)
                continue;
            bool isValid = info->maxInputChannels > 0;
            if (isValid && info->hostApi == hostApi)
            {
                bool isLoopback = false;
#if defined(_WIN32) && defined(PA_USE_WASAPI)
                isLoopback = PaWasapi_IsLoopback(i) == 1;
#endif
                devices.push_back({i, info->name, isLoopback});
            }
        }

        return devices;
    }

    int Recorder::audioCallback(const void *inputBuffer, void *outputBuffer,
                                const unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData)
    {
        const auto recorder = static_cast<Recorder *>(userData);
        if (const auto input = static_cast<const float *>(inputBuffer))
        {
            std::lock_guard lock(recorder->audioBufferMutex);
            const int channels = std::max(1, recorder->channelCount);
            for (unsigned long frame = 0; frame < framesPerBuffer; ++frame)
            {
                const float left = input[frame * channels];
                const float right = channels > 1 ? input[frame * channels + 1] : left;
                recorder->audioBuffer.push_back({left, right});
                ++recorder->capturedFrameSequence;
            }
            while (recorder->audioBuffer.size() > MAX_BUFFER_FRAMES)
                recorder->audioBuffer.pop_front();
        }
        return paContinue;
    }

    bool Recorder::startRecording(int deviceIndex)
    {
        // Atomically claim recording ownership to prevent concurrent entry
        bool expected = false;
        if (!recording.compare_exchange_strong(expected, true))
        {
            spdlog::warn("Already recording. Aborting...");
            return false;
        }

        {
            std::lock_guard lock(audioBufferMutex);
            audioBuffer.clear();
            capturedFrameSequence = 0;
            lastDeliveredSequence = 0;
        }

        const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);
        if (!info)
        {
            spdlog::warn("Couldn't get device info for index {}. Aborting...", deviceIndex);
            recording = false;
            return false;
        }

        bool isLoopback = false;
#if defined(_WIN32) && defined(PA_USE_WASAPI)
        isLoopback = PaWasapi_IsLoopback(deviceIndex) == 1;
#endif

        PaStreamParameters inputParams;
        inputParams.device = deviceIndex;
        channelCount = std::clamp(info->maxInputChannels, 1, 2);
        inputParams.channelCount = channelCount;
        inputParams.sampleFormat = paFloat32;
        inputParams.suggestedLatency = info->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;

#if defined(_WIN32) && defined(PA_USE_WASAPI)
        // For loopback devices, enable WASAPI auto-conversion to handle
        // channel count and sample rate differences between our request and the output device
        PaWasapiStreamInfo wasapiInfo = {};
        if (isLoopback)
        {
            wasapiInfo.size = sizeof(PaWasapiStreamInfo);
            wasapiInfo.hostApiType = paWASAPI;
            wasapiInfo.version = 1;
            wasapiInfo.flags = paWinWasapiAutoConvert;
            inputParams.hostApiSpecificStreamInfo = &wasapiInfo;
        }
#endif

        spdlog::info("Trying to open device {}: {} at {} Hz with {} channels{}",
            deviceIndex, info->name, info->defaultSampleRate, inputParams.channelCount,
            isLoopback ? " (loopback)" : "");

        PaError formatResult = Pa_IsFormatSupported(&inputParams, nullptr, info->defaultSampleRate);
        if (formatResult != paFormatIsSupported)
        {
            spdlog::error("Format isn't supported for device {}: {}(code: {})", deviceIndex, Pa_GetErrorText(formatResult), formatResult);
            recording = false;
            return false;
        }

        sampleRate = info->defaultSampleRate;

        PaError err = Pa_OpenStream(&stream,
                                    &inputParams,
                                    nullptr,
                                    info->defaultSampleRate,
                                    paFramesPerBufferUnspecified,
                                    paNoFlag,
                                    audioCallback,
                                    this);
        if (err != paNoError)
        {
            spdlog::error("Failed to open stream: {}", Pa_GetErrorText(err));
            stream = nullptr;
            recording = false;
            return false;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
            spdlog::error("Failed to start stream: {}", Pa_GetErrorText(err));
            Pa_CloseStream(stream);
            stream = nullptr;
            recording = false;
            return false;
        }

        spdlog::info("Recording started on device {} at {} Hz{}", deviceIndex, sampleRate, isLoopback ? " (loopback)" : "");
        currentDeviceIndex = deviceIndex;
        return true;
    }

    bool Recorder::isDefaultOutputLoopbackAvailable()
    {
#if defined(_WIN32) && defined(PA_USE_WASAPI)
        return getDefaultOutputLoopbackIndex() >= 0;
#elif defined(__linux__)
        // GNOME on modern distributions normally runs PipeWire with its
        // PulseAudio compatibility server. parec understands the special
        // @DEFAULT_MONITOR@ source and follows the active default sink.
        return executableOnPath("parec");
#else
        return false;
#endif
    }

    bool Recorder::startDefaultOutputLoopback()
    {
#if defined(_WIN32) && defined(PA_USE_WASAPI)
        const int index = getDefaultOutputLoopbackIndex();
        return index >= 0 && startRecording(index);
#elif defined(__linux__)
        bool expected = false;
        if (!recording.compare_exchange_strong(expected, true))
        {
            spdlog::warn("Already recording. Aborting...");
            return false;
        }

        if (!isDefaultOutputLoopbackAvailable())
        {
            spdlog::error("Linux desktop loopback requires 'parec' (usually provided by pulseaudio-utils). PipeWire's PulseAudio compatibility service must also be running.");
            recording = false;
            return false;
        }

        int pipeFds[2] = {-1, -1};
        if (::pipe2(pipeFds, O_CLOEXEC) != 0)
        {
            spdlog::error("Failed to create loopback pipe: {}", std::strerror(errno));
            recording = false;
            return false;
        }

        const pid_t pid = ::fork();
        if (pid < 0)
        {
            spdlog::error("Failed to start parec: {}", std::strerror(errno));
            ::close(pipeFds[0]);
            ::close(pipeFds[1]);
            recording = false;
            return false;
        }

        if (pid == 0)
        {
            ::dup2(pipeFds[1], STDOUT_FILENO);
            ::close(pipeFds[0]);
            ::close(pipeFds[1]);
            ::execlp("parec", "parec",
                     "--device=@DEFAULT_MONITOR@",
                     "--format=float32le",
                     "--rate=44100",
                     "--channels=2",
                     "--latency-msec=10",
                     "--process-time-msec=5",
                     "--raw",
                     static_cast<char *>(nullptr));
            _exit(127);
        }

        ::close(pipeFds[1]);
        loopbackPipeFd = pipeFds[0];
        loopbackPid = pid;
        currentDeviceIndex = -2;
        sampleRate = 44100.0;
        channelCount = 2;
        stopLoopbackThread = false;
        {
            std::lock_guard lock(audioBufferMutex);
            audioBuffer.clear();
            capturedFrameSequence = 0;
            lastDeliveredSequence = 0;
        }
        loopbackThread = std::thread(&Recorder::linuxLoopbackReadLoop, this);
        spdlog::info("Recording Linux desktop output through PipeWire/PulseAudio default monitor");
        return true;
#else
        return false;
#endif
    }

#ifndef _WIN32
    void Recorder::linuxLoopbackReadLoop()
    {
        // 128 stereo frames (256 floats) keeps the pipe responsive without
        // waking for every tiny server fragment.
        std::array<float, 256> interleaved{};
        while (!stopLoopbackThread)
        {
            pollfd descriptor{loopbackPipeFd, POLLIN, 0};
            const int ready = ::poll(&descriptor, 1, 100);
            if (ready < 0)
            {
                if (errno == EINTR)
                    continue;
                break;
            }
            if (ready == 0)
                continue;
            if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) && !(descriptor.revents & POLLIN))
                break;

            const ssize_t bytes = ::read(loopbackPipeFd, interleaved.data(), sizeof(interleaved));
            if (bytes <= 0)
            {
                if (bytes < 0 && (errno == EINTR || errno == EAGAIN))
                    continue;
                break;
            }

            const size_t floatCount = static_cast<size_t>(bytes) / sizeof(float);
            const size_t frameCount = floatCount / 2;
            std::lock_guard lock(audioBufferMutex);
            for (size_t frame = 0; frame < frameCount; ++frame)
            {
                audioBuffer.push_back({interleaved[frame * 2], interleaved[frame * 2 + 1]});
                ++capturedFrameSequence;
            }
            while (audioBuffer.size() > MAX_BUFFER_FRAMES)
                audioBuffer.pop_front();
        }

        if (!stopLoopbackThread)
        {
            spdlog::error("Linux desktop loopback capture stopped unexpectedly");
            recording = false;
        }
    }

#endif

    void Recorder::stopRecording()
    {
        if (!recording && !stream
#ifndef _WIN32
            && loopbackPid <= 0 && !loopbackThread.joinable()
#endif
        )
            return;
        if (stream)
        {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
#ifndef _WIN32
        stopLoopbackThread = true;
        if (loopbackPid > 0)
            ::kill(loopbackPid, SIGTERM);
        if (loopbackThread.joinable())
            loopbackThread.join();
        if (loopbackPipeFd >= 0)
        {
            ::close(loopbackPipeFd);
            loopbackPipeFd = -1;
        }
        if (loopbackPid > 0)
        {
            int status = 0;
            ::waitpid(loopbackPid, &status, 0);
            loopbackPid = -1;
        }
#endif
        spdlog::info("Recording stopped");
        recording = false;
        currentDeviceIndex = -1;

        // We are not clearing up here as there might be issues because audio data is still processed, instead clearing it in startRecording
    }

    bool Recorder::isRecording() const
    {
        return recording;
    }

    double Recorder::getSampleRate() const
    {
        return sampleRate;
    }

    size_t Recorder::getBufferedFrameCount() const
    {
        std::lock_guard lock(audioBufferMutex);
        return audioBuffer.size();
    }

    double Recorder::getBufferedLatencyMs() const
    {
        std::lock_guard lock(audioBufferMutex);
        if (sampleRate <= 0.0) return 0.0;
        return static_cast<double>(audioBuffer.size()) * 1000.0 / sampleRate;
    }

    int Recorder::getDefaultOutputLoopbackIndex()
    {
#if defined(_WIN32) && defined(PA_USE_WASAPI)
spdlog::info("Checking for default output loopback device...");
        // Ensure PortAudio updates the WASAPI device list so new loopback devices are visible
        PaError err = PaWasapi_UpdateDeviceList();
        if (err != paNoError)
        {
            spdlog::error("Failed to update WASAPI device list: {}", Pa_GetErrorText(err));
            return -1;
        }

        PaDeviceIndex defaultOutput = Pa_GetDefaultOutputDevice();
        if (defaultOutput == paNoDevice)
            return -1;

            spdlog::info("Default output device index: {}", defaultOutput);
        const PaDeviceInfo *outputInfo = Pa_GetDeviceInfo(defaultOutput);
        if (!outputInfo)
            return -1;

        std::string outputName = outputInfo->name;
        int hostApi = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);

        // PortAudio creates loopback devices with the name "<output device name> [Loopback]"
        // Find the loopback device that matches the default output device's name
        const int numDevices = Pa_GetDeviceCount();
        for (int i = 0; i < numDevices; ++i)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (!info || info->hostApi != hostApi || info->maxInputChannels <= 0)
                continue;

            if (PaWasapi_IsLoopback(i) == 1)
            {
                std::string loopbackName = info->name;
                // Check if this loopback device corresponds to our default output
                // PA names loopback devices as "<name> [Loopback]"
                if (loopbackName.find(outputName) == 0) {
                    spdlog::info("Found loopback device '{}' for default output '{}'", loopbackName, outputName);
                    return i;
                }
            }
        }
#endif
        return -1;
    }

    std::optional<CapturedAudioFrame> Recorder::getLastSamples()
    {
        std::lock_guard lock(audioBufferMutex);
        if (audioBuffer.size() < MUSIC_ANALYSIS_WINDOW_SIZE)
            return std::nullopt;
        if (capturedFrameSequence - lastDeliveredSequence < FFT_HOP_SIZE)
            return std::nullopt;

        CapturedAudioFrame result;
        result.sampleRate = sampleRate;
        result.sequence = capturedFrameSequence;
        result.mono.resize(MUSIC_ANALYSIS_WINDOW_SIZE);
        result.left.resize(MUSIC_ANALYSIS_WINDOW_SIZE);
        result.right.resize(MUSIC_ANALYSIS_WINDOW_SIZE);

        auto it = audioBuffer.end() - static_cast<std::ptrdiff_t>(MUSIC_ANALYSIS_WINDOW_SIZE);
        for (size_t i = 0; i < MUSIC_ANALYSIS_WINDOW_SIZE; ++i, ++it)
        {
            result.left[i] = it->left;
            result.right[i] = it->right;
            result.mono[i] = 0.5f * (it->left + it->right);
        }
        lastDeliveredSequence = capturedFrameSequence;
        return result;
    }
}

#include "record.h"

#ifdef _WIN32
#include <windows.h>

#ifdef PA_USE_WASAPI
#include <pa_win_wasapi.h>
#endif

#endif

#include <spdlog/spdlog.h>

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
        std::unique_lock<std::mutex> lock(recorder->audioBufferMutex);

        spdlog::trace("Audio callback called with {} frames", framesPerBuffer);
        if (const auto input = static_cast<const float *>(inputBuffer))
        {
            // Add new samples to buffer
            for (unsigned long i = 0; i < framesPerBuffer; ++i)
            {
                recorder->audioBuffer.push_back(input[i]);
            }

            // Keep buffer size manageable
            while (recorder->audioBuffer.size() > MAX_BUFFER_SIZE)
            {
                recorder->audioBuffer.pop_front();
            }
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

        this->audioBuffer.clear();

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
        inputParams.channelCount = 1;
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

    void Recorder::stopRecording()
    {
        if (!recording)
            return;
        if (stream)
        {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
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

    int Recorder::getDefaultOutputLoopbackIndex()
    {
#if defined(_WIN32) && defined(PA_USE_WASAPI)
        PaDeviceIndex defaultOutput = Pa_GetDefaultOutputDevice();
        if (defaultOutput == paNoDevice)
            return -1;

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
                if (loopbackName.find(outputName) == 0)
                    return i;
            }
        }
#endif
        return -1;
    }

    std::optional<std::vector<float>> Recorder::getLastSamples()
    {
        std::unique_lock<std::mutex> lock(audioBufferMutex);
        if (audioBuffer.size() < FFT_SIZE)
            return std::nullopt;


        std::vector<float> samples(audioBuffer.end() - FFT_SIZE, audioBuffer.end());
        audioBuffer.erase(audioBuffer.end() - FFT_SIZE, audioBuffer.end());

        return samples;
    }
}

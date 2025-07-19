#include "record.h"
#ifdef _WIN32
#include <windows.h>
#include <portaudio.h>
#include <iostream>
#ifdef PA_USE_WASAPI
#include <pa_win_wasapi.h>
#endif
#else
#include <portaudio.h>
#include <iostream>
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
        int numDevices = Pa_GetDeviceCount();
        for (int i = 0; i < numDevices; ++i)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (info && info->maxOutputChannels > 0)
            {
                devices.push_back({i, info->name});
            }
        }
        return devices;
    }

    int Recorder::audioCallback(const void *inputBuffer, void *outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData)
    {
        Recorder *recorder = static_cast<Recorder *>(userData);
        const float *input = static_cast<const float *>(inputBuffer);

        if (input)
        {
            std::lock_guard<std::mutex> lock(recorder->bufferMutex);

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
        if (recording) {
            spdlog::warn("Already recording. Aborting...");
            return false;
        }
        const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);
        if (!info) {
            spdlog::warn("Couldn't get device info for index {}. Aborting...", deviceIndex);
            return false;
        }

        // Find a supported sample rate
        double rate = info->defaultSampleRate;
        PaStreamParameters inputParams;
        inputParams.device = deviceIndex;
        inputParams.channelCount = 1;
        inputParams.sampleFormat = paFloat32;
        inputParams.suggestedLatency = info->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;

        spdlog::info("Trying to open device {}: {} at {} Hz", deviceIndex, info->name, rate);
        PaError formatResult = Pa_IsFormatSupported(&inputParams, nullptr, rate);
        if (formatResult != paFormatIsSupported)
            {
            spdlog::error("No supported sample rate for device {}: {}(code: {})", deviceIndex, Pa_GetErrorText(formatResult), formatResult);
            return false;
        }

        sampleRate = rate;

#ifdef _WIN32
#ifdef PA_USE_WASAPI
        // WASAPI loopback for output device
        inputParams.hostApiSpecificStreamInfo = PaWasapi_GetLoopbackStreamInfo();
#endif
#endif

        PaError err = Pa_OpenStream(reinterpret_cast<PaStream **>(&stream),
                                    &inputParams,
                                    nullptr,
                                    rate,
                                    paFramesPerBufferUnspecified,
                                    paNoFlag,
                                    audioCallback,
                                    this);
        if (err != paNoError)
        {
            spdlog::error("Failed to open stream: {}", Pa_GetErrorText(err));
            stream = nullptr;
            return false;
        }

        err = Pa_StartStream(reinterpret_cast<PaStream *>(stream));
        if (err != paNoError)
        {
            spdlog::error("Failed to start stream: {}", Pa_GetErrorText(err));
            Pa_CloseStream(reinterpret_cast<PaStream *>(stream));
            stream = nullptr;
            return false;
        }

        spdlog::info("Recording started on device {} at {} Hz", deviceIndex, sampleRate);
        recording = true;
        currentDeviceIndex = deviceIndex;
        return true;
    }

    void Recorder::stopRecording()
    {
        if (!recording)
            return;
        if (stream)
        {
            Pa_StopStream(reinterpret_cast<PaStream *>(stream));
            Pa_CloseStream(reinterpret_cast<PaStream *>(stream));
            stream = nullptr;
        }
        spdlog::info("Recording stopped");
        recording = false;
        currentDeviceIndex = -1;

        // Clear audio buffer
        std::lock_guard<std::mutex> lock(bufferMutex);
        audioBuffer.clear();
    }

    bool Recorder::isRecording() const
    {
        return recording;
    }

    std::vector<float> Recorder::getLatestSamples(size_t numSamples)
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        std::vector<float> samples;

        if (audioBuffer.size() >= numSamples)
        {
            samples.reserve(numSamples);
            auto it = audioBuffer.end() - numSamples;
            samples.insert(samples.end(), it, audioBuffer.end());
        }
        else
        {
            // Return all available samples, padded with zeros if needed
            samples.reserve(numSamples);
            samples.insert(samples.end(), audioBuffer.begin(), audioBuffer.end());
            samples.resize(numSamples, 0.0f);
        }

        return samples;
    }

    double Recorder::getSampleRate() const
    {
        return sampleRate;
    }
}

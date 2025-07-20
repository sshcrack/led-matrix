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
        audioChannel = std::make_shared<Channel<std::vector<float>>>();
    }

    Recorder::~Recorder()
    {
        if (recording)
            stopRecording();
        Pa_Terminate();
        audioChannel->close();
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
                devices.push_back({i, info->name});
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

        // Process audio buffer in chunks to avoid lag
        while (recorder->audioBuffer.size() >= FFT_SIZE) {
            const std::vector newAudioBuffer(recorder->audioBuffer.begin(), recorder->audioBuffer.begin() + FFT_SIZE);
            recorder->audioBuffer.erase(recorder->audioBuffer.begin(), recorder->audioBuffer.begin() + FFT_SIZE);

            recorder->audioChannel->send(newAudioBuffer);
        }

        return paContinue;
    }

    bool Recorder::startRecording(int deviceIndex)
    {
        if (recording)
        {
            spdlog::warn("Already recording. Aborting...");
            return false;
        }

        this->audioBuffer.clear();

        const PaDeviceInfo *info = Pa_GetDeviceInfo(deviceIndex);
        if (!info)
        {
            spdlog::warn("Couldn't get device info for index {}. Aborting...", deviceIndex);
            return false;
        }

        // Find a supported sample rate
        double rate = info->defaultSampleRate;
        PaStreamParameters inputParams;
        inputParams.device = deviceIndex;
        inputParams.channelCount = info->maxInputChannels;
        inputParams.sampleFormat = paFloat32;
        inputParams.suggestedLatency = info->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;

        spdlog::info("Trying to open device {}: {} at {} Hz with {} channels", deviceIndex, info->name, rate, inputParams.channelCount);
        PaError formatResult = Pa_IsFormatSupported(&inputParams, nullptr, rate);
        if (formatResult != paFormatIsSupported)
        {
            spdlog::error("Format isn't supported for device {}: {}(code: {})", deviceIndex, Pa_GetErrorText(formatResult), formatResult);
            return false;
        }

        sampleRate = rate;

        PaError err = Pa_OpenStream(&stream,
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

        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
            spdlog::error("Failed to start stream: {}", Pa_GetErrorText(err));
            Pa_CloseStream(stream);
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

    std::shared_ptr<Channel<std::vector<float>>> Recorder::getChannel() const {
        return audioChannel;
    }
}

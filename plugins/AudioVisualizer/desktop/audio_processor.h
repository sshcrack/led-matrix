#pragma once
#include <vector>
#include <fftw3.h>
#include <memory>
#include <thread>
#include <expected>
#include "config.h"
#include "frequency_analyzer/factory.h"
#include "record.h"

class AudioProcessor {
public:

    AudioProcessor(AudioVisualizerConfig& config, uint32_t sampleRate);
    [[nodiscard]] std::vector<float> getBands();
    [[nodiscard]] bool getInterpolatedLog() const;

    std::expected<void, std::string> startProcessingThread(int deviceIdx);
    void stopProcessingThread() {
        threadRunning = false;
    }

    std::vector<float> getLatestBands() {
        std::lock_guard lock(bandsMutex);
        return currentBands_;
    }

    [[nodiscard]] bool isThreadRunning() const { return threadRunning; }


    std::vector<AudioRecorder::Recorder::DeviceInfo> listDevices() {
        return recorder->listDevices();
    }
private:
    void threadFunction();

    std::vector<float> computeFFT(const std::vector<float>& samples);
    std::vector<float> computeBands(const std::vector<float>& spectrum) const;
    void applyAmplitudeProcessing(std::vector<float> &bands) const;

    std::unique_ptr<fftwf_complex[]> fftInput_;
    std::unique_ptr<fftwf_complex[]> fftOutput_;
    fftwf_plan fftPlan_;
    std::vector<float> window_;
    AudioVisualizerConfig &config_;
    uint32_t sampleRate_;

    std::mutex bandsMutex;
    std::vector<float> currentBands_;


    std::unique_ptr<FrequencyAnalyzer> analyzer;
    std::unique_ptr<AudioRecorder::Recorder> recorder;


    bool threadRunning = false;
    std::thread processingThread;
};

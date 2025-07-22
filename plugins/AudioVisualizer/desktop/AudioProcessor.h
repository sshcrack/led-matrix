#pragma once
#include <vector>
#include <fftw3.h>
#include <memory>
#include <thread>
#include <expected>
#include <mutex>
#include <string>
#include "config.h"
#include "frequency_analyzer/factory.h"
#include "record.h"

class AudioProcessor
{
public:
    AudioProcessor(AudioVisualizerConfig &config);

    [[nodiscard]] std::vector<float> getBands();
    [[nodiscard]] bool getInterpolatedLog() const;


    void updateAnalyzer();

    std::vector<float> computeBands(const std::vector<float> &rawSamples, double sampleRate);
private:
    std::vector<float> computeFFT(const std::vector<float> samples);
    void applyAmplitudeProcessing(std::vector<float> &bands, const std::vector<float> &prevBands) const;

    std::unique_ptr<fftwf_complex[]> fftInput_;
    std::unique_ptr<fftwf_complex[]> fftOutput_;
    fftwf_plan fftPlan_;

    std::vector<float> window_;
    AudioVisualizerConfig &config_;

    std::mutex bandsMutex;
    std::vector<float> currentBands_;

    std::mutex analyzerMutex;
    std::unique_ptr<FrequencyAnalyzer> analyzer;
};

#pragma once
#include <vector>
#include <deque>
#include <fftw3.h>
#include <memory>
#include "config.h" // Placeholder for config struct/class

class AudioProcessor {
public:
    static constexpr size_t BUFFER_SIZE = 2048;
    static constexpr size_t FFT_SIZE = 1024;

    AudioProcessor(AudioVisualizerConfig& config, uint32_t sampleRate);
    void processAudio(const std::vector<float>& samples);
    std::vector<float> getBands() const;
    bool getInterpolatedLog() const;

private:
    void computeFFT();
    void computeBands();
    void applyAmplitudeProcessing(const std::vector<float>& bands);

    std::unique_ptr<fftwf_complex[]> fftInput_;
    std::unique_ptr<fftwf_complex[]> fftOutput_;
    fftwf_plan fftPlan_;
    std::vector<float> window_;
    std::deque<float> buffer_;
    std::vector<float> spectrum_;
    std::vector<float> bands_;
    AudioVisualizerConfig &config_;
    uint32_t sampleRate_;
};

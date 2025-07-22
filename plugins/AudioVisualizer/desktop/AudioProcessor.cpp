#include "AudioProcessor.h"
#include <cmath>
#include <algorithm>
#include <expected>
#include <iostream>
#include <shared/desktop/config.h>
#include <spdlog/spdlog.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264f
#endif

AudioProcessor::AudioProcessor(AudioVisualizerConfig &config)
    : fftInput_(new fftwf_complex[FFT_SIZE]),
      fftOutput_(new fftwf_complex[FFT_SIZE]),
      window_(FFT_SIZE),
      config_(config)
{
    // Create Hann window
    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        const float angle = 2.0f * M_PI * i / (FFT_SIZE - 1);
        window_[i] = 0.5f * (1.0f - std::cos(angle));
    }
    fftPlan_ = fftwf_plan_dft_1d(FFT_SIZE, fftInput_.get(), fftOutput_.get(), FFTW_FORWARD, FFTW_MEASURE);

    analyzer = getAnalyzer(config.analysisMode, config.frequencyScale);
}

std::vector<float> AudioProcessor::computeFFT(const std::vector<float> samples)
{
    if (samples.size() < FFT_SIZE)
    {
        return {}; // Not enough data for FFT
    }

    // Prepare input with windowing
    auto it = samples.end() - FFT_SIZE; // Take the last FFT_SIZE samples
    for (size_t i = 0; i < FFT_SIZE; ++i, ++it)
    {
        fftInput_[i][0] = *it * window_[i]; // real part with windowing
        fftInput_[i][1] = 0.0f;             // imaginary part
    }

    fftwf_execute(fftPlan_);

    std::vector spectrum(FFT_SIZE / 2, 0.0f);
    // Compute magnitude spectrum
    constexpr float normalization = 1.0f / FFT_SIZE;
    constexpr float windowCorrection = 2.0f;

    for (size_t i = 0; i < FFT_SIZE / 2; ++i)
    {
        const float re = fftOutput_[i][0];
        const float im = fftOutput_[i][1];
        spectrum[i] = std::sqrt(re * re + im * im) * normalization * windowCorrection;
    }

    return spectrum;
}

void AudioProcessor::applyAmplitudeProcessing(std::vector<float> &bands, const std::vector<float> &prevBands) const
{
    for (size_t i = 0; i < bands.size(); ++i)
    {
        if (i >= prevBands.size())
            continue; // Skip if index is out of bounds

        float bandEnergy = bands[i];

        // Apply gain
        bandEnergy *= config_.gain;
        float processed = 0.0f;

        // Apply amplitude scaling based on linear_amplitude setting
        if (config_.linearAmplitudeScaling)
        {
            // Linear amplitude: just normalize to 0-1 range
            processed = std::min(bandEnergy, 1.0f);
        }
        else
        {
            // Logarithmic amplitude: convert to dB scale
            if (bandEnergy > 0.0f)
            {
                // Convert to dB scale: 20 * log10(amplitude)
                // Add a small offset to avoid log(0) and normalize to 0-1 range
                const float db = 20.0f * std::log10(bandEnergy + 1e-10f);
                processed = std::max(0.0f, std::min((db + 60.0f) / 60.0f, 1.0f));
            }
        }

        // Apply smoothing
        bands[i] = prevBands[i] * config_.smoothing + processed * (1.0f - config_.smoothing);
    }
}

std::vector<float> AudioProcessor::getBands()
{
    std::lock_guard lock(bandsMutex);
    std::vector<float> bands = currentBands_;

    return bands;
}

bool AudioProcessor::getInterpolatedLog() const
{
    const bool interpolated = config_.interpolateMissingBands;
    const bool frequencyScaleLog = config_.frequencyScale == Logarithmic;

    return interpolated && frequencyScaleLog;
}

void AudioProcessor::updateAnalyzer()
{
    std::lock_guard lock(analyzerMutex);
    analyzer = getAnalyzer(config_.analysisMode, config_.frequencyScale);
}

std::vector<float> AudioProcessor::computeBands(const std::vector<float> &rawSamples, double sampleRate)
{
    auto spectrum = computeFFT(rawSamples);
    if (spectrum.empty())
        return spectrum;

    const float freqResolution = static_cast<float>(sampleRate) / FFT_SIZE;
    const auto minBin = static_cast<size_t>(config_.minFreq / freqResolution);
    const size_t maxBin = std::min(static_cast<size_t>(config_.maxFreq / freqResolution), spectrum.size() - 1);

    std::lock_guard lock1(analyzerMutex);
    auto bands = analyzer->computeBands(spectrum, config_, freqResolution, minBin, maxBin);

    std::lock_guard lock(bandsMutex);
    applyAmplitudeProcessing(bands, currentBands_);

    currentBands_ = bands;

    return bands;
}

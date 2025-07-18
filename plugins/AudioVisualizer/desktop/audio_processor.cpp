#include "audio_processor.h"
#include <cmath>
#include <algorithm>
#include <iostream>

#define M_PI 3.14159265358979323846264f

AudioProcessor::AudioProcessor(AudioVisualizerConfig &config, uint32_t sampleRate)
    : fftInput_(new fftwf_complex[FFT_SIZE]),
      fftOutput_(new fftwf_complex[FFT_SIZE]),
      window_(FFT_SIZE),
      spectrum_(FFT_SIZE / 2, 0.0f),
      bands_(config.numBands, 0.0f),
      config_(config),
      sampleRate_(sampleRate)
{
    // Create Hann window
    for (size_t i = 0; i < FFT_SIZE; ++i)
    {
        float angle = 2.0f * M_PI * i / (FFT_SIZE - 1);
        window_[i] = 0.5f * (1.0f - std::cos(angle));
    }
    fftPlan_ = fftwf_plan_dft_1d(FFT_SIZE, fftInput_.get(), fftOutput_.get(), FFTW_FORWARD, FFTW_MEASURE);
}

void AudioProcessor::processAudio(const std::vector<float> &samples)
{
    for (float sample : samples)
    {
        buffer_.push_back(sample);
        if (buffer_.size() > BUFFER_SIZE)
            buffer_.pop_front();
    }
    if (buffer_.size() >= FFT_SIZE)
    {
        computeFFT();
        computeBands();
    }
}

void AudioProcessor::computeFFT()
{
    if (buffer_.size() < FFT_SIZE) {
        return; // Not enough data for FFT
    }

    // Prepare input with windowing
    auto it = buffer_.end() - FFT_SIZE; // Take the last FFT_SIZE samples
    for (size_t i = 0; i < FFT_SIZE; ++i, ++it)
    {
        fftInput_[i][0] = *it * window_[i]; // real part with windowing
        fftInput_[i][1] = 0.0f;             // imaginary part
    }
    
    fftwf_execute(fftPlan_);
    
    // Compute magnitude spectrum
    float normalization = 1.0f / FFT_SIZE;
    float windowCorrection = 2.0f;
    for (size_t i = 0; i < FFT_SIZE / 2; ++i)
    {
        float re = fftOutput_[i][0];
        float im = fftOutput_[i][1];
        spectrum_[i] = std::sqrt(re * re + im * im) * normalization * windowCorrection;
    }
}

void AudioProcessor::computeBands()
{
    float freqResolution = static_cast<float>(sampleRate_) / FFT_SIZE;
    size_t minBin = static_cast<size_t>(config_.minFreq / freqResolution);
    size_t maxBin = std::min(static_cast<size_t>(config_.maxFreq / freqResolution), spectrum_.size() - 1);
    if (maxBin <= minBin)
        return;

    // Placeholder: frequency analyzer logic
    std::vector<float> bands(config_.numBands, 0.0f);
    // Simple linear band mapping for demonstration
    size_t binsPerBand = (maxBin - minBin + 1) / config_.numBands;
    for (size_t band = 0; band < config_.numBands; ++band)
    {
        float sum = 0.0f;
        for (size_t bin = minBin + band * binsPerBand; bin < minBin + (band + 1) * binsPerBand && bin <= maxBin; ++bin)
        {
            sum += spectrum_[bin];
        }
        bands[band] = sum / binsPerBand;
    }
    applyAmplitudeProcessing(bands);
}

void AudioProcessor::applyAmplitudeProcessing(const std::vector<float> &bands)
{
    for (size_t i = 0; i < std::min(bands.size(), bands_.size()); ++i)
    {
        float bandEnergy = bands[i];
        bandEnergy *= config_.gain;
        float processed = 0.0f;

        if (config_.linearAmplitudeScaling)
        {
            processed = std::min(bandEnergy, 1.0f);
        }
        else
        {
            if (bandEnergy > 0.0f)
            {
                float db = 20.0f * std::log10(bandEnergy + 1e-10f);
                processed = std::max(0.0f, std::min((db + 60.0f) / 60.0f, 1.0f));
            }
            else
            {
                processed = 0.0f;
            }
        }
        bands_[i] = bands_[i] * config_.smoothing + processed * (1.0f - config_.smoothing);
    }
}

std::vector<float> AudioProcessor::getBands() const
{
    return bands_;
}

bool AudioProcessor::getInterpolatedLog() const
{
    bool interpolated = config_.interpolateMissingBands;
    bool frequencyScaleLog = config_.frequencyScale == FrequencyScale::Logarithmic;

    return interpolated && frequencyScaleLog;
}

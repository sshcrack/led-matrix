#include "audio_processor.h"
#include <cmath>
#include <algorithm>
#include <expected>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846264f
#endif

AudioProcessor::AudioProcessor(AudioVisualizerConfig &config, uint32_t sampleRate)
    : fftInput_(new fftwf_complex[FFT_SIZE]),
      fftOutput_(new fftwf_complex[FFT_SIZE]),
      window_(FFT_SIZE),
      config_(config),
      sampleRate_(sampleRate) {
    // Create Hann window
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        const float angle = 2.0f * M_PI * i / (FFT_SIZE - 1);
        window_[i] = 0.5f * (1.0f - std::cos(angle));
    }
    fftPlan_ = fftwf_plan_dft_1d(FFT_SIZE, fftInput_.get(), fftOutput_.get(), FFTW_FORWARD, FFTW_MEASURE);
}

std::vector<float> AudioProcessor::computeFFT(const std::vector<float> &samples) {
    if (samples.size() < FFT_SIZE) {
        return {}; // Not enough data for FFT
    }

    // Prepare input with windowing
    auto it = samples.end() - FFT_SIZE; // Take the last FFT_SIZE samples
    for (size_t i = 0; i < FFT_SIZE; ++i, ++it) {
        fftInput_[i][0] = *it * window_[i]; // real part with windowing
        fftInput_[i][1] = 0.0f; // imaginary part
    }

    fftwf_execute(fftPlan_);

    std::vector spectrum(FFT_SIZE / 2, 0.0f);
    // Compute magnitude spectrum
    constexpr float normalization = 1.0f / FFT_SIZE;
    constexpr float windowCorrection = 2.0f;

    for (size_t i = 0; i < FFT_SIZE / 2; ++i) {
        const float re = fftOutput_[i][0];
        const float im = fftOutput_[i][1];
        spectrum[i] = std::sqrt(re * re + im * im) * normalization * windowCorrection;
    }

    return spectrum;
}

std::vector<float> AudioProcessor::computeBands(const std::vector<float> &spectrum) const {
    const float freqResolution = static_cast<float>(sampleRate_) / FFT_SIZE;
    const size_t minBin = static_cast<size_t>(config_.minFreq / freqResolution);
    const size_t maxBin = std::min(static_cast<size_t>(config_.maxFreq / freqResolution), spectrum.size() - 1);
    if (maxBin <= minBin)
        return {};

    // Placeholder: frequency analyzer logic
    std::vector bands(config_.numBands, 0.0f);
    // Simple linear band mapping for demonstration
    const size_t binsPerBand = (maxBin - minBin + 1) / config_.numBands;
    for (size_t band = 0; band < config_.numBands; ++band) {
        float sum = 0.0f;
        for (size_t bin = minBin + band * binsPerBand; bin < minBin + (band + 1) * binsPerBand && bin <= maxBin; ++
             bin) {
            sum += spectrum[bin];
        }
        bands[band] = sum / binsPerBand;
    }
    applyAmplitudeProcessing(bands);

    return bands;
}

void AudioProcessor::applyAmplitudeProcessing(std::vector<float> &bands) const {
    for (size_t i = 0; i < bands.size(); ++i) {
        float bandEnergy = bands[i];
        bandEnergy *= config_.gain;
        float processed = 0.0f;

        if (config_.linearAmplitudeScaling) {
            processed = std::min(bandEnergy, 1.0f);
        } else {
            if (bandEnergy > 0.0f) {
                float db = 20.0f * std::log10(bandEnergy + 1e-10f);
                processed = std::max(0.0f, std::min((db + 60.0f) / 60.0f, 1.0f));
            } else {
                processed = 0.0f;
            }
        }
        bands[i] = bands[i] * config_.smoothing + processed * (1.0f - config_.smoothing);
    }
}

std::vector<float> AudioProcessor::getBands() {
    std::lock_guard lock(bandsMutex);
    std::vector<float> bands = currentBands_;

    return bands;
}

bool AudioProcessor::getInterpolatedLog() const {
    const bool interpolated = config_.interpolateMissingBands;
    const bool frequencyScaleLog = config_.frequencyScale == Logarithmic;

    return interpolated && frequencyScaleLog;
}

void AudioProcessor::threadFunction() {
    const auto channel = recorder->getChannel();
    while (threadRunning) {
        const auto samples = channel->receive();
        if (!samples.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Sleep if no samples available
            continue;
        }

        auto fft = computeFFT(samples.value());
        if (fft.empty()) {
            continue; // Skip if not enough data for FFT
        }

        auto spectrum = computeBands(fft);
        if (spectrum.empty()) {
            continue; // Skip if no valid bands computed
        }


        // Compute bands using analyzer
        {
            const float freqResolution = static_cast<float>(recorder->getSampleRate()) / FFT_SIZE;
            const size_t minBin = static_cast<size_t>(config_.minFreq / freqResolution);
            const size_t maxBin = std::min(static_cast<size_t>(config_.maxFreq / freqResolution), spectrum.size() - 1);

            const auto bands = analyzer->computeBands(spectrum, config_, freqResolution, minBin, maxBin);
            std::lock_guard lock(bandsMutex);
            currentBands_ = bands;
        }
    }

    recorder->stopRecording();
}

std::expected<void, std::string> AudioProcessor::startProcessingThread(const int deviceIdx) {
    if (threadRunning)
        return std::unexpected("Processing thread is already running");

    if (recorder->isRecording())
        return std::unexpected("Recorder is already running");

    recorder->startRecording(deviceIdx);
    threadRunning = true;
    this->processingThread = std::thread(&AudioProcessor::threadFunction, this);

    return {};
}

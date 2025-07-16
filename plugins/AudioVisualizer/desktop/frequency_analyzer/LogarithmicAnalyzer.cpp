#include "LogarithmicAnalyzer.h"
#include <cmath>
#include <atomic>

std::vector<float> LogarithmicAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    float freqResolution,
    size_t minBin,
    size_t maxBin
) const {
    std::vector<float> bands;
    bands.reserve(config.numBands);
    // For simplicity, skip atomic interpolate_bands, use config.interpolateBands
    bool shouldInterpolate = config.interpolateMissingBands;
    std::vector<size_t> skipped;

    float logMinFreq = std::log(config.minFreq);
    float logMaxFreq = std::log(config.maxFreq);
    float logFreqRange = logMaxFreq - logMinFreq;

    for (size_t i = 0; i < config.numBands; ++i) {
        float bandStartRatio = static_cast<float>(i) / config.numBands;
        float bandEndRatio = static_cast<float>(i + 1) / config.numBands;
        float bandStartLogFreq = logMinFreq + bandStartRatio * logFreqRange;
        float bandEndLogFreq = logMinFreq + bandEndRatio * logFreqRange;
        float bandStartFreq = std::exp(bandStartLogFreq);
        float bandEndFreq = std::exp(bandEndLogFreq);
        size_t startBin = std::max(static_cast<size_t>(bandStartFreq / freqResolution), minBin);
        size_t endBin = std::min(static_cast<size_t>(bandEndFreq / freqResolution), maxBin);
        if (endBin <= startBin) {
            skipped.push_back(i);
            if (shouldInterpolate || !config.skipMissingBandsFromOutput) {
                bands.push_back(0.0f);
            }
        } else {
            float bandEnergy = 0.0f;
            for (size_t j = startBin; j < endBin; ++j) {
                bandEnergy += spectrum[j];
            }
            bandEnergy /= (endBin - startBin);
            bands.push_back(bandEnergy);
        }
    }
    // Interpolation pass
    if (shouldInterpolate) {
        size_t prevValidIdx = -1;
        for (size_t i = 0; i < config.numBands; ++i) {
            if (std::find(skipped.begin(), skipped.end(), i) != skipped.end()) {
                size_t nextValidIdx = -1;
                for (size_t j = i + 1; j < config.numBands; ++j) {
                    if (std::find(skipped.begin(), skipped.end(), j) == skipped.end()) {
                        nextValidIdx = j;
                        break;
                    }
                }
                if (prevValidIdx != (size_t)-1 && nextValidIdx != (size_t)-1) {
                    float ratio = float(i - prevValidIdx) / float(nextValidIdx - prevValidIdx);
                    bands[i] = bands[prevValidIdx] * (1.0f - ratio) + bands[nextValidIdx] * ratio;
                } else if (prevValidIdx != (size_t)-1) {
                    bands[i] = bands[prevValidIdx] * 0.9f;
                } else if (nextValidIdx != (size_t)-1) {
                    bands[i] = bands[nextValidIdx] * 0.9f;
                } else {
                    bands[i] = 0.1f;
                }
            } else {
                prevValidIdx = i;
            }
        }
    }
    return bands;
}

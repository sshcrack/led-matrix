#include "LogarithmicAnalyzer.h"
#include <cmath>
#include <atomic>

auto LogarithmicAnalyzer::computeBands(
    const std::vector<float> &spectrum,
    const AudioVisualizerConfig &config,
    const float freqResolution,
    const size_t minBin,
    const size_t maxBin
) const -> std::vector<float> {
    std::vector<float> bands;
    bands.reserve(config.numBands);
    // For simplicity, skip atomic interpolate_bands, use config.interpolateBands
    const bool shouldInterpolate = config.interpolateMissingBands;
    std::vector<size_t> skipped;

    const float logMinFreq = std::log(config.minFreq);
    const float logMaxFreq = std::log(config.maxFreq);
    const float logFreqRange = logMaxFreq - logMinFreq;

    for (size_t i = 0; i < config.numBands; ++i) {
        const float bandStartRatio = static_cast<float>(i) / config.numBands;
        const float bandEndRatio = static_cast<float>(i + 1) / config.numBands;
        const float bandStartLogFreq = logMinFreq + bandStartRatio * logFreqRange;
        const float bandEndLogFreq = logMinFreq + bandEndRatio * logFreqRange;
        const float bandStartFreq = std::exp(bandStartLogFreq);
        const float bandEndFreq = std::exp(bandEndLogFreq);


        const size_t startBin = std::max(static_cast<size_t>(bandStartFreq / freqResolution), minBin);
        const size_t endBin = std::min(static_cast<size_t>(bandEndFreq / freqResolution), maxBin);
        if (endBin <= startBin) {
            skipped.push_back(i);
            if (shouldInterpolate || !config.skipMissingBandsFromOutput) {
                bands.push_back(0.0f);
            }
            continue;
        }

        float bandEnergy = 0.0f;
        for (size_t j = startBin; j < endBin; ++j) {
            bandEnergy += spectrum[j];
        }
        bandEnergy /= (endBin - startBin);
        bands.push_back(bandEnergy);
    }

    // Interpolation pass
    if (shouldInterpolate) {
        size_t prevValidIdx = -1;
        for (size_t i = 0; i < config.numBands; ++i) {
            if (std::ranges::find(skipped, i) != skipped.end()) {
                size_t nextValidIdx = -1;
                for (size_t j = i + 1; j < config.numBands; ++j) {
                    if (std::ranges::find(skipped, j) == skipped.end()) {
                        nextValidIdx = j;
                        break;
                    }
                }

                size_t invalidIdx = -1;
                if (prevValidIdx != invalidIdx && nextValidIdx != invalidIdx) {
                    const float ratio = static_cast<float>(i - prevValidIdx) / static_cast<float>(nextValidIdx - prevValidIdx);
                    bands[i] = bands[prevValidIdx] * (1.0f - ratio) + bands[nextValidIdx] * ratio;
                } else if (prevValidIdx != invalidIdx) {
                    bands[i] = bands[prevValidIdx] * 0.9f;
                } else if (nextValidIdx != invalidIdx) {
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

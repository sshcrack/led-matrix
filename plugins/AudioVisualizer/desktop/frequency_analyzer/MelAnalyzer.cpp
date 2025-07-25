#include "MelAnalyzer.h"
#include <cmath>

std::vector<float> MelAnalyzer::computeBands(
    const std::vector<float> &spectrum,
    const AudioVisualizerConfig &config,
    const float freqResolution,
    const size_t minBin,
    const size_t maxBin
) const {
    std::vector bands(config.numBands, 0.0f);
    const float minMel = 2595.0f * std::log10(1.0f + config.minFreq / 700.0f);
    const float maxMel = 2595.0f * std::log10(1.0f + config.maxFreq / 700.0f);
    for (size_t i = 0; i < config.numBands; ++i) {
        const float bandRatio = static_cast<float>(i) / (config.numBands - 1);
        const float targetMel = minMel + bandRatio * (maxMel - minMel);
        const float targetFreq = 700.0f * (std::pow(10.0f, targetMel / 2595.0f) - 1.0f);
        const size_t targetBin = targetFreq / freqResolution;
        const size_t startBin = (i == 0) ? minBin : targetBin > 3 ? targetBin - 3 : minBin;
        const size_t endBin = (i == config.numBands - 1) ? maxBin : std::min(targetBin + 3, maxBin);

        if (endBin <= startBin) continue;

        float bandEnergy = 0.0f;
        for (size_t j = startBin; j < endBin; ++j) {
            bandEnergy += spectrum[j];
        }

        bandEnergy /= (endBin - startBin);
        bands[i] = bandEnergy;
    }
    return bands;
}

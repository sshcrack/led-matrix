#include "BarkAnalyzer.h"
#include <cmath>

std::vector<float> BarkAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    const float freqResolution,
    const size_t minBin,
    const size_t maxBin
) const {
    std::vector<float> bands;
    bands.reserve(config.numBands);
    auto hzToBark = [](const float freq) { return (26.81f * freq) / (1960.0f + freq) - 0.53f; };
    auto barkToHz = [](const float bark) { return 1960.0f / (26.81f / (bark + 0.53f) - 1.0f); };

    const float minBark = hzToBark(config.minFreq);
    const float maxBark = hzToBark(config.maxFreq);
    const float barkWidth = (maxBark - minBark) / config.numBands;
    for (size_t i = 0; i < config.numBands; ++i) {
        const float barkStart = minBark + barkWidth * i;
        const float barkEnd = minBark + barkWidth * (i + 1);
        const float freqStart = barkToHz(barkStart);
        const float freqEnd = barkToHz(barkEnd);
        const size_t binStart = std::max(static_cast<size_t>(freqStart / freqResolution), minBin);
        const size_t binEnd = std::min(static_cast<size_t>(freqEnd / freqResolution), maxBin);

        if (binEnd <= binStart) {
            if (!config.skipMissingBandsFromOutput) bands.push_back(0.0f);
            continue;
        }
        float bandEnergy = 0.0f;
        for (size_t j = binStart; j < binEnd; ++j) {
            bandEnergy += spectrum[j];
        }
        bandEnergy /= (binEnd - binStart);
        bands.push_back(bandEnergy);
    }
    return bands;
}

#include "BarkAnalyzer.h"
#include <cmath>

std::vector<float> BarkAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    float freqResolution,
    size_t minBin,
    size_t maxBin
) const {
    std::vector<float> bands;
    bands.reserve(config.numBands);
    auto hzToBark = [](float freq) { return (26.81f * freq) / (1960.0f + freq) - 0.53f; };
    auto barkToHz = [](float bark) { return 1960.0f / (26.81f / (bark + 0.53f) - 1.0f); };
    float minBark = hzToBark(config.minFreq);
    float maxBark = hzToBark(config.maxFreq);
    float barkWidth = (maxBark - minBark) / config.numBands;
    for (size_t i = 0; i < config.numBands; ++i) {
        float barkStart = minBark + barkWidth * i;
        float barkEnd = minBark + barkWidth * (i + 1);
        float freqStart = barkToHz(barkStart);
        float freqEnd = barkToHz(barkEnd);
        size_t binStart = std::max(static_cast<size_t>(freqStart / freqResolution), minBin);
        size_t binEnd = std::min(static_cast<size_t>(freqEnd / freqResolution), maxBin);
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

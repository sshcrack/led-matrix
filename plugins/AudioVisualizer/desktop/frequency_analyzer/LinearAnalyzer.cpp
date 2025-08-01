#include "LinearAnalyzer.h"

std::vector<float> LinearAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    float /*freqResolution*/,
    const size_t minBin,
    const size_t maxBin
) const {
    std::vector<float> bands(config.numBands, 0.0f);
    const size_t binsPerBand = (maxBin - minBin) / std::max<size_t>(config.numBands, 1);
    for (size_t i = 0; i < config.numBands; ++i) {
        const size_t startBin = minBin + i * binsPerBand;
        const size_t endBin = std::min(startBin + binsPerBand, maxBin);

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

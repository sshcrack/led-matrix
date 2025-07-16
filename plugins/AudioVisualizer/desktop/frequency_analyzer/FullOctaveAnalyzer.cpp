#include "FullOctaveAnalyzer.h"
#include "utils.h"
#include <cmath>

std::vector<float> FullOctaveAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    float freqResolution,
    size_t /*minBin*/,
    size_t /*maxBin*/
) const {
    auto octaveCenters = generateOctaveCenters(config);
    std::vector<float> bands(std::min(octaveCenters.size(), size_t(config.numBands)), 0.0f);
    for (size_t i = 0; i < bands.size(); ++i) {
        float centerFreq = octaveCenters[i];
        if (centerFreq < config.minFreq || centerFreq > config.maxFreq) continue;
        float lowerFreq = centerFreq / std::sqrt(2.0f);
        float upperFreq = centerFreq * std::sqrt(2.0f);
        size_t lowerBin = static_cast<size_t>(lowerFreq / freqResolution);
        size_t upperBin = static_cast<size_t>(upperFreq / freqResolution);
        if (upperBin <= lowerBin) continue;
        float bandEnergy = 0.0f;
        for (size_t j = lowerBin; j < std::min(upperBin, spectrum.size()); ++j) {
            bandEnergy += spectrum[j];
        }
        bandEnergy /= (upperBin - lowerBin);
        bands[i] = bandEnergy;
    }
    return bands;
}

#include "FullOctaveAnalyzer.h"
#include "utils.h"
#include <cmath>

std::vector<float> FullOctaveAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    const float freqResolution,
    size_t /*minBin*/,
    size_t /*maxBin*/
) const {
    const auto octaveCenters = generateOctaveCenters(config);
    std::vector bands(std::min(octaveCenters.size(), static_cast<size_t>(config.numBands)), 0.0f);
    for (size_t i = 0; i < bands.size(); ++i) {
        const float centerFreq = octaveCenters[i];
        if (centerFreq < config.minFreq || centerFreq > config.maxFreq) continue;

        const float lowerFreq = centerFreq / std::sqrt(2.0f);
        const float upperFreq = centerFreq * std::sqrt(2.0f);
        const size_t lowerBin = lowerFreq / freqResolution;

        size_t upperBin = upperFreq / freqResolution;
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

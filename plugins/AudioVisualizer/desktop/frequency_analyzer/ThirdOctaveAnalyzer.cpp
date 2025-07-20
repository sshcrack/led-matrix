#include "ThirdOctaveAnalyzer.h"
#include "utils.h"
#include <cmath>

std::vector<float> ThirdOctaveAnalyzer::computeBands(
    const std::vector<float>& spectrum,
    const AudioVisualizerConfig& config,
    const float freqResolution,
    size_t /*minBin*/,
    size_t /*maxBin*/
) const {
    const auto thirdOctaveCenters = generateThirdOctaveCenters(config);
    std::vector<float> bands(std::min(thirdOctaveCenters.size(), size_t(config.numBands)), 0.0f);
    for (size_t i = 0; i < bands.size(); ++i) {
        const float centerFreq = thirdOctaveCenters[i];
        if (centerFreq < config.minFreq || centerFreq > config.maxFreq) continue;

        const float lowerFreq = centerFreq / std::pow(2.0f, 1.0f / 6.0f);
        const float upperFreq = centerFreq * std::pow(2.0f, 1.0f / 6.0f);

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

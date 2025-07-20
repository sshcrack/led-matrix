#include "utils.h"

std::vector<float> generateThirdOctaveCenters(const AudioVisualizerConfig& config) {
    static constexpr float baseFrequencies[] = {
        31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f, 200.0f, 250.0f, 315.0f, 400.0f, 500.0f,
        630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f,
        8000.0f, 10000.0f, 12500.0f, 16000.0f, 20000.0f
    };

    std::vector<float> result;
    for (float freq : baseFrequencies) {
        if (freq >= config.minFreq && freq <= config.maxFreq) {
            result.push_back(freq);
        }
    }
    return result;
}

std::vector<float> generateOctaveCenters(const AudioVisualizerConfig& config) {
    static constexpr float baseFrequencies[] = {
        31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };
    std::vector<float> result;
    for (float freq : baseFrequencies) {
        if (freq >= config.minFreq && freq <= config.maxFreq) {
            result.push_back(freq);
        }
    }
    return result;
}

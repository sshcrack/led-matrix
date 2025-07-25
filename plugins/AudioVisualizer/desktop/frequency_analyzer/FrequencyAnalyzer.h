#pragma once
#include <vector>
#include "../config.h"

class FrequencyAnalyzer {
public:
    virtual ~FrequencyAnalyzer() = default;
    virtual std::vector<float> computeBands(
        const std::vector<float>& spectrum,
        const AudioVisualizerConfig& config,
        float freqResolution,
        size_t minBin,
        size_t maxBin
    ) const = 0;
};

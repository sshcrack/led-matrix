#pragma once
#include "FrequencyAnalyzer.h"

class LinearAnalyzer : public FrequencyAnalyzer {
public:
    std::vector<float> computeBands(
        const std::vector<float>& spectrum,
        const AudioVisualizerConfig& config,
        float freqResolution,
        size_t minBin,
        size_t maxBin
    ) const override;
};

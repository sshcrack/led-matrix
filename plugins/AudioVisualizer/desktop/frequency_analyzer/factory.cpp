#include "factory.h"

std::unique_ptr<FrequencyAnalyzer> getAnalyzer(AnalysisMode mode, FrequencyScale frequencyScale) {
    switch (mode) {
        case OneThirdOctaveBands:
            return std::make_unique<ThirdOctaveAnalyzer>();
        case FullOctave:
            return std::make_unique<FullOctaveAnalyzer>();
        case DiscreteFrequencies:
        default:
            switch (frequencyScale) {
                case Linear:
                    return std::make_unique<LinearAnalyzer>();
                case Bark:
                    return std::make_unique<BarkAnalyzer>();
                case Mel:
                    return std::make_unique<MelAnalyzer>();
                case Logarithmic:
                default:
                    return std::make_unique<LogarithmicAnalyzer>();
            }
    }
}

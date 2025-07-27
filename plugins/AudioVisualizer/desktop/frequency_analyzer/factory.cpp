#include "factory.h"

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4244)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#endif

#include "LogarithmicAnalyzer.h"
#include "LinearAnalyzer.h"
#include "BarkAnalyzer.h"
#include "MelAnalyzer.h"
#include "ThirdOctaveAnalyzer.h"
#include "FullOctaveAnalyzer.h"

#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

std::unique_ptr<FrequencyAnalyzer> getAnalyzer(const AnalysisMode mode, const FrequencyScale frequencyScale) {
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

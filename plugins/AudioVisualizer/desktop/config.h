#pragma once
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum AnalysisMode
{
    DiscreteFrequencies,
    OneThirdOctaveBands,
    FullOctave
};

enum FrequencyScale
{
    Linear,
    Logarithmic,
    Bark,
    Mel
};

// Make sure these are in the same order as the enum values
std::vector<std::string> analysisModes = {"Discrete Frequencies", "1/3 Octave Bands", "Full Octave Bands"};
std::vector<std::string> frequencyScales = {"Linear", "Logarithmic", "Bark", "Mel"};


AnalysisMode to_analysis_mode(const std::string &str)
{
    if (str == "Discrete Frequencies") return DiscreteFrequencies;
    if (str == "1/3 Octave Bands") return OneThirdOctaveBands;
    if (str == "Full Octave Bands") return FullOctave;
    throw std::invalid_argument("Unknown AnalysisMode: " + str);
}

FrequencyScale to_frequency_scale(const std::string &str)
{
    if (str == "Linear") return Linear;
    if (str == "Logarithmic") return Logarithmic;
    if (str == "Bark") return Bark;
    if (str == "Mel") return Mel;
    throw std::invalid_argument("Unknown FrequencyScale: " + str);
}


NLOHMANN_JSON_SERIALIZE_ENUM(AnalysisMode, {{DiscreteFrequencies, "Discrete Frequencies"},
                                            {OneThirdOctaveBands, "1/3 Octave Bands"},
                                            {FullOctave, "Full Octave Bands"}})

NLOHMANN_JSON_SERIALIZE_ENUM(FrequencyScale, {{Linear, "Linear"},
                                              {Logarithmic, "Logarithmic"},
                                              {Bark, "Bark"},
                                              {Mel, "Mel"}})

class AudioVisualizerConfig
{
public:
    int port;

    // ---- Audio Settings ---

    int numBands;
    double gain;
    double smoothing;
    double minFreq;
    double maxFreq;

    // Analysis settings
    AnalysisMode analysisMode;
    FrequencyScale frequencyScale;
    bool linearAmplitudeScaling;
    bool interpolateMissingBands;
    bool skipMissingBandsFromOutput;

    AudioVisualizerConfig() : port(8888), numBands(64), gain(2.0), smoothing(0.8), minFreq(20.0), maxFreq(20000.0),
                              analysisMode(DiscreteFrequencies), frequencyScale(Logarithmic), skipMissingBandsFromOutput(true) {}
};

void from_json(const json &j, AudioVisualizerConfig &config)
{
    j.at("port").get_to(config.port);
    j.at("numBands").get_to(config.numBands);
    j.at("gain").get_to(config.gain);
    j.at("smoothing").get_to(config.smoothing);
    j.at("minFreq").get_to(config.minFreq);
    j.at("maxFreq").get_to(config.maxFreq);
    j.at("analysisMode").get_to(config.analysisMode);
    j.at("frequencyScale").get_to(config.frequencyScale);
    j.at("linearAmplitudeScaling").get_to(config.linearAmplitudeScaling);
    j.at("interpolateMissingBands").get_to(config.interpolateMissingBands);
    j.at("skipMissingBandsFromOutput").get_to(config.skipMissingBandsFromOutput);
}

void to_json(json &j, const AudioVisualizerConfig &config)
{
    j = json{
        {"port", config.port},
        {"numBands", config.numBands},
        {"gain", config.gain},
        {"smoothing", config.smoothing},
        {"minFreq", config.minFreq},
        {"maxFreq", config.maxFreq},
        {"analysisMode", config.analysisMode},
        {"frequencyScale", config.frequencyScale},
        {"linearAmplitudeScaling", config.linearAmplitudeScaling},
        {"interpolateMissingBands", config.interpolateMissingBands},
        {"skipMissingBandsFromOutput", config.skipMissingBandsFromOutput}};
}
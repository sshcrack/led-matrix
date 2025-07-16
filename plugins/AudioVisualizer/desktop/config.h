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
    if (str == "Discrete Frequencies")
        return DiscreteFrequencies;
    if (str == "1/3 Octave Bands")
        return OneThirdOctaveBands;
    if (str == "Full Octave Bands")
        return FullOctave;
    throw std::invalid_argument("Unknown AnalysisMode: " + str);
}

FrequencyScale to_frequency_scale(const std::string &str)
{
    if (str == "Linear")
        return Linear;
    if (str == "Logarithmic")
        return Logarithmic;
    if (str == "Bark")
        return Bark;
    if (str == "Mel")
        return Mel;
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

    // ----- Audio Device Settings -----
    std::string deviceName;

    AudioVisualizerConfig() : port(8888), numBands(64), gain(2.0), smoothing(0.8), minFreq(20.0), maxFreq(20000.0),
                              analysisMode(DiscreteFrequencies), frequencyScale(Logarithmic), skipMissingBandsFromOutput(true) {}
};

void from_json(const json &j, AudioVisualizerConfig &config)
{
    AudioVisualizerConfig defaults;
    config.port = j.value("port", defaults.port);
    config.numBands = j.value("numBands", defaults.numBands);
    config.gain = j.value("gain", defaults.gain);
    config.smoothing = j.value("smoothing", defaults.smoothing);
    config.minFreq = j.value("minFreq", defaults.minFreq);
    config.maxFreq = j.value("maxFreq", defaults.maxFreq);
    config.analysisMode = j.value("analysisMode", defaults.analysisMode);
    config.frequencyScale = j.value("frequencyScale", defaults.frequencyScale);
    config.linearAmplitudeScaling = j.value("linearAmplitudeScaling", defaults.linearAmplitudeScaling);
    config.interpolateMissingBands = j.value("interpolateMissingBands", defaults.interpolateMissingBands);
    config.skipMissingBandsFromOutput = j.value("skipMissingBandsFromOutput", defaults.skipMissingBandsFromOutput);
    config.deviceName = j.value("deviceName", defaults.deviceName);
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
        {"skipMissingBandsFromOutput", config.skipMissingBandsFromOutput},
        {"deviceName", config.deviceName}};
}
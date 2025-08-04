#pragma once
#include <nlohmann/json.hpp>
#include "BeatDetector.h"

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
static std::vector<std::string> analysisModes = {"Discrete Frequencies", "1/3 Octave Bands", "Full Octave Bands"};
static std::vector<std::string> frequencyScales = {"Linear", "Logarithmic", "Bark", "Mel"};
static std::vector<std::string> beatDetectionAlgorithms = {"Energy", "Spectral Flux", "High Frequency Content", "Complex Domain"};

static AnalysisMode to_analysis_mode(const std::string &str)
{
    if (str == "Discrete Frequencies")
        return DiscreteFrequencies;
    if (str == "1/3 Octave Bands")
        return OneThirdOctaveBands;
    if (str == "Full Octave Bands")
        return FullOctave;
    throw std::invalid_argument("Unknown AnalysisMode: " + str);
}

static FrequencyScale to_frequency_scale(const std::string &str)
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

static BeatDetectionAlgorithm to_beat_detection_algorithm(const std::string &str)
{
    if (str == "Energy")
        return BeatDetectionAlgorithm::Energy;
    if (str == "Spectral Flux")
        return BeatDetectionAlgorithm::SpectralFlux;
    if (str == "High Frequency Content")
        return BeatDetectionAlgorithm::HighFrequencyContent;
    if (str == "Complex Domain")
        return BeatDetectionAlgorithm::ComplexDomain;
    throw std::invalid_argument("Unknown BeatDetectionAlgorithm: " + str);
}

class AudioVisualizerConfig
{
public:
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
    
    // ----- Beat Detection Settings -----
    BeatDetectionConfig beatDetection;

    AudioVisualizerConfig() : numBands(64), gain(2.0), smoothing(0.8), minFreq(20.0), maxFreq(20000.0),
                              analysisMode(DiscreteFrequencies), frequencyScale(Logarithmic),
                              skipMissingBandsFromOutput(true), linearAmplitudeScaling(false), interpolateMissingBands(false) {}
};

NLOHMANN_JSON_SERIALIZE_ENUM(AnalysisMode, {{DiscreteFrequencies, "Discrete Frequencies"},
                                            {OneThirdOctaveBands, "1/3 Octave Bands"},
                                            {FullOctave, "Full Octave Bands"}})

NLOHMANN_JSON_SERIALIZE_ENUM(FrequencyScale, {{Linear, "Linear"},
                                              {Logarithmic, "Logarithmic"},
                                              {Bark, "Bark"},
                                              {Mel, "Mel"}})

NLOHMANN_JSON_SERIALIZE_ENUM(BeatDetectionAlgorithm, {{BeatDetectionAlgorithm::Energy, "Energy"},
                                                      {BeatDetectionAlgorithm::SpectralFlux, "Spectral Flux"},
                                                      {BeatDetectionAlgorithm::HighFrequencyContent, "High Frequency Content"},
                                                      {BeatDetectionAlgorithm::ComplexDomain, "Complex Domain"}})

static void from_json(const json &j, BeatDetectionConfig &config)
{
    BeatDetectionConfig defaults;
    config.algorithm = j.value("algorithm", defaults.algorithm);
    config.energyThreshold = j.value("energyThreshold", defaults.energyThreshold);
    config.minTimeBetweenBeats = j.value("minTimeBetweenBeats", defaults.minTimeBetweenBeats);
    config.historySize = j.value("historySize", defaults.historySize);
    config.focusBands = j.value("focusBands", defaults.focusBands);
    config.spectralFluxThreshold = j.value("spectralFluxThreshold", defaults.spectralFluxThreshold);
    config.hfcThreshold = j.value("hfcThreshold", defaults.hfcThreshold);
}

static void to_json(json &j, const BeatDetectionConfig &config)
{
    j = json{
        {"algorithm", config.algorithm},
        {"energyThreshold", config.energyThreshold},
        {"minTimeBetweenBeats", config.minTimeBetweenBeats},
        {"historySize", config.historySize},
        {"focusBands", config.focusBands},
        {"spectralFluxThreshold", config.spectralFluxThreshold},
        {"hfcThreshold", config.hfcThreshold}};
}

static void from_json(const json &j, AudioVisualizerConfig &config)
{
    AudioVisualizerConfig defaults;
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
    config.beatDetection = j.value("beatDetection", defaults.beatDetection);
}

static void to_json(json &j, const AudioVisualizerConfig &config)
{
    j = json{
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
        {"deviceName", config.deviceName},
        {"beatDetection", config.beatDetection}};
}
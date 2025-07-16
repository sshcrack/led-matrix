#pragma once
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class AudioVisualizerConfig
{
public:
    int port;
    AudioVisualizerConfig() : port(8888) {} // Default port
};

void from_json(const json &j, AudioVisualizerConfig &config)
{
    if (j.contains("port"))
        config.port = j.at("port").get<int>();
}

void to_json(json &j, const AudioVisualizerConfig &config)
{
    j = {
        {"port", config.port} //
    };
}
#pragma once
#include "shared/desktop/plugin/main.h"
#include "config.h"
#include "record.h"
#include "audio_processor.h"
#include "frequency_analyzer/factory.h"
#include "frequency_analyzer/FrequencyAnalyzer.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include "../../../thirdparty/implot/implot.h"

enum ConnectionStatus
{
    Disconnected,
    Connected,
    Error
};

std::string to_string(ConnectionStatus status)
{
    switch (status)
    {
    case Disconnected:
        return "Disconnected";
    case Connected:
        return "Connected";
    case Error:
        return "Error";
    default:
        return "Unknown";
    }
}

class AudioVisualizerDesktop final : public Plugins::DesktopPlugin
{
public:
    AudioVisualizerDesktop();


    ~AudioVisualizerDesktop() override;

    void render(ImGuiContext *ctx) override;
    void loadConfig(std::optional<const nlohmann::json> config) override
    {
        if (config.has_value())
            cfg = config.value();
    };
    void saveConfig(nlohmann::json &config) const override
    {
        config = cfg;
    };

private:
    ImPlotContext *implotContext = nullptr;

    AudioVisualizerConfig cfg;

    std::unique_ptr<AudioProcessor> audioProcessor;
    std::vector<float> latestBands;
    std::string lastError;

protected:
    void addConnectionSettings();
    void addAnalysisSettings();
    void addAudioSettings();
    void addDeviceSettings();
    void addSpectrumSettings();
};

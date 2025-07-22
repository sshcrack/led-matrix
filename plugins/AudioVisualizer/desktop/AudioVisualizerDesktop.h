#pragma once
#include "shared/desktop/plugin/main.h"
#include "config.h"
#include "record.h"
#include "AudioProcessor.h"
#include "frequency_analyzer/factory.h"
#include "frequency_analyzer/FrequencyAnalyzer.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include "../../../thirdparty/implot/implot.h"

class AudioVisualizerDesktop final : public Plugins::DesktopPlugin
{
public:
    AudioVisualizerDesktop();

    ~AudioVisualizerDesktop() override;

    void render(ImGuiContext *ctx) override;
    void loadConfig(std::optional<const nlohmann::json> config) override;
    void saveConfig(nlohmann::json &config) const override
    {
        config = cfg;
    };

    void beforeExit() override;
    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> onNextPacket(std::string sceneName) override;

private:
    ImPlotContext *implotContext = nullptr;

    AudioVisualizerConfig cfg;

    std::unique_ptr<AudioProcessor> audioProcessor;
    std::unique_ptr<AudioRecorder::Recorder> recorder;

    std::mutex latestBandsMutex;
    std::vector<float> latestBands;

    std::shared_mutex lastErrorMutex;
    std::string lastError;

protected:
    void addConnectionSettings();
    void addAnalysisSettings();
    void addAudioSettings();
    void addDeviceSettings();
    void addVisualizer();
};
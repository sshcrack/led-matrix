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

    void render() override;
    void load_config(std::optional<const nlohmann::json> config) override;
    void save_config(nlohmann::json &config) const override
    {
        config = cfg;
    };

    void before_exit() override;
    void post_init() override;
    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> compute_next_packet(std::string sceneName) override;

    std::string get_plugin_name() const override {
        return PLUGIN_NAME;
    }

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
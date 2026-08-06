#pragma once

#include "AudioProcessor.h"
#include "MusicAnalyzer.h"
#include "config.h"
#include "frequency_analyzer/FrequencyAnalyzer.h"
#include "frequency_analyzer/factory.h"
#include "record.h"
#include <implot.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared/desktop/plugin/main.h>
#include <shared_mutex>
#include <string>
#include <vector>

class AudioVisualizerDesktop final : public Plugins::DesktopPlugin {
public:
    AudioVisualizerDesktop();
    ~AudioVisualizerDesktop() override;

    void render() override;
    void load_config(std::optional<const nlohmann::json> config) override;
    void save_config(nlohmann::json &config) const override { config = cfg; }
    void before_exit() override;
    void post_init() override;
    std::optional<std::unique_ptr<UdpPacket>> compute_next_packet(std::string sceneName) override;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
    void initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc *alloc_fn,
                          ImGuiMemFreeFunc *free_fn, void **user_data) override;

private:
    ImPlotContext *implotContext = nullptr;
    AudioVisualizerConfig cfg;
    std::unique_ptr<AudioProcessor> audioProcessor;
    std::unique_ptr<MusicAnalyzer> musicAnalyzer;
    std::unique_ptr<AudioRecorder::Recorder> recorder;

    std::vector<float> latestBands;
    AudioProtocol::Frame latestAnalysis;
    bool hasLatestAnalysis = false;

    mutable std::shared_mutex lastErrorMutex;
    std::string lastError;
    std::mutex stateMutex;
    std::string currentDeviceName;

protected:
    void addConnectionSettings();
    void addAnalysisSettings();
    void addAudioSettings();
    void addDeviceSettings();
    void addMusicAnalysisSettings();
    void addVisualizer();
};

#pragma once
#include "shared/desktop/plugin/main.h"
#include "config.h"
#include <nlohmann/json.hpp>
#include <memory>


class AudioVisualizerDesktop final : public Plugins::DesktopPlugin {
public:
    AudioVisualizerDesktop();

    ~AudioVisualizerDesktop() override;

    void render(ImGuiContext *ctx) override;
    void loadConfig(const nlohmann::json& config) override {
        pluginConfig = config;
    };
    void saveConfig(nlohmann::json& config) const override {
        config = pluginConfig;
    };

private:
    AudioVisualizerConfig pluginConfig;

    bool autostart = false;
    bool minimizeToToolbar = false;
};

#pragma once
#include "shared/desktop/plugin/main.h"
#include <nlohmann/json.hpp>
#include <memory>


class AudioVisualizerDesktop final : public Plugins::DesktopPlugin {
public:
    AudioVisualizerDesktop();

    ~AudioVisualizerDesktop() override;

    void render(ImGuiContext *ctx) override;
    void loadConfig(const nlohmann::json& config) override;
    void saveConfig(nlohmann::json& config) const override;

private:
    nlohmann::json pluginConfig;

    bool autostart = false;
    bool minimizeToToolbar = false;
};

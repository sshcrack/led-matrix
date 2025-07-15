#pragma once
#include "shared/desktop/plugin/main.h"


class AudioVisualizerDesktop final : public Plugins::DesktopPlugin {
public:
    AudioVisualizerDesktop();

    ~AudioVisualizerDesktop() override;

    void render(ImGuiContext *ctx) override;
};

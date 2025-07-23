#pragma once
#include "shared/desktop/plugin/main.h"
#include <nlohmann/json.hpp>
#include <memory>

class ShadertoyDesktop final : public Plugins::DesktopPlugin
{
public:
    ShadertoyDesktop() = default;

    void render(ImGuiContext *ctx) override;
    void on_websocket_message(std::string message) override;

    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> compute_next_packet(std::string sceneName) override;
    std::string get_plugin_name() const override {
        return PLUGIN_NAME;
    }
private:
    int width, height;
    std::string url;
};
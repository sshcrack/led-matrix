#pragma once
#include "shared/desktop/plugin/main.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <shadertoy/ShaderToyContext.hpp>

class ShadertoyDesktop final : public Plugins::DesktopPlugin
{
public:
    ShadertoyDesktop() = default;

    void render() override;
    void on_websocket_message(std::string message) override;

    ShaderToy::ShaderToyContext ctx;
    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> compute_next_packet(std::string sceneName) override;
    std::string get_plugin_name() const override {
        return PLUGIN_NAME;
    }

    void post_init() override;
    void after_swap(ImGuiContext *imCtx) override;

private:
    int width, height;
    std::string url;
    // Default is false, still waiting for the WebsocketClient to send the URL
    bool hasUrlChanged = false;

    std::string initError;

    bool enablePreview;

    std::shared_mutex currDataMutex;
    std::vector<uint8_t> currData;
};

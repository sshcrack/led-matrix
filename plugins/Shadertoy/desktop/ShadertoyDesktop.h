#pragma once
#include "shared/desktop/plugin/main.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <shadertoy/ShaderToyContext.hpp>
#include "ShaderCache.h"

class ShadertoyDesktop final : public Plugins::DesktopPlugin
{
public:
    ShadertoyDesktop() = default;
    ~ShadertoyDesktop() override;

    void render() override;
    void on_websocket_message(std::string message) override;

    ShaderToy::ShaderToyContext ctx;
    std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> compute_next_packet(std::string sceneName) override;
    std::string get_plugin_name() const override {
        return PLUGIN_NAME;
    }

    void post_init() override;
    void after_swap(ImGuiContext *imCtx) override;
    void initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc*alloc_fn, ImGuiMemFreeFunc*free_fn, void **user_data) override;

private:
    int width, height;
    std::string url;
    // Default is false, still waiting for the WebsocketClient to send the URL
    bool hasUrlChanged = false;

    std::string initError;

    bool enablePreview;

    std::shared_mutex currDataMutex;
    std::vector<uint8_t> currData;

    // Cache system
    std::unique_ptr<ShaderCache> mCache;
    
    // Custom cache entry UI
    bool mShowCacheEditor = false;
    char mCacheKeyInput[256] = {0};
    char mCacheFileInput[1024] = {0};
    std::string mCacheToDelete;
    
    void renderCacheEditorUI();
    void loadCacheFromUrl(const std::string& url);
};

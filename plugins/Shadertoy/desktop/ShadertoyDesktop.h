#pragma once
#include "shared/desktop/plugin/main.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <cstdint>
#include <shadertoy/ShaderToy.hpp>
#include "ShaderCache.h"

class ShadertoyDesktop final : public Plugins::DesktopPlugin
{
public:
    ShadertoyDesktop() = default;
    ~ShadertoyDesktop() override;

    void render() override;
    void on_websocket_message(std::string message) override;

    ShaderToy::Runtime runtime;
    std::optional<std::unique_ptr<UdpPacket>> compute_next_packet(std::string sceneName) override;
    std::string get_plugin_name() const override {
        return PLUGIN_NAME;
    }

    void post_init() override;
    void after_swap(ImGuiContext *imCtx) override;
    [[nodiscard]] bool is_large_payload_plugin() const override { return true; }
    void initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc*alloc_fn, ImGuiMemFreeFunc*free_fn, void **user_data) override;

private:
    // size: messages arrive on the WebSocket message thread; the UI and UDP
    // sender threads read these.
    std::atomic<int> width{0};
    std::atomic<int> height{0};

    // url/custom_shader_*/hasUrlChanged are written by on_websocket_message()
    // (WebsocketClient plugin-message thread) and read by render()/after_swap()
    // (UI thread). Concurrent std::string assign/read is a use-after-free, so
    // every access goes through shaderStateMutex_. shaderStateGeneration_ is
    // bumped on each update so a finished load only clears hasUrlChanged if no
    // newer request arrived while it was compiling.
    mutable std::mutex shaderStateMutex_;
    std::string url;
    std::string custom_shader_code;
    std::string custom_shader_name;
    // Default is false, still waiting for the WebsocketClient to send the URL
    bool hasUrlChanged = false;
    std::uint64_t shaderStateGeneration_ = 0;

    std::string initError;

    bool enablePreview = false;
    std::atomic<bool> shaderRequested{false};

    std::shared_mutex currDataMutex;
    std::vector<uint8_t> currData;

    // Cache system
    std::unique_ptr<ShaderCache> mCache;
    
    // Custom cache entry UI
    bool mShowCacheEditor = false;
    char mCacheKeyInput[256] = {0};
    char mCacheValueInput[4096] = {0};
    std::string mCacheToDelete;
    
    static void renderCanvasCallback(const ImDrawList* draw_list, const ImDrawCmd* command);

    void renderCacheEditorUI();
    void loadCacheFromUrl(const std::string& shaderUrl, std::uint64_t generation);
    void loadLocalShaderFromCode(const std::string& name, const std::string& code, std::uint64_t generation);
    void markShaderLoaded(std::uint64_t generation);
};

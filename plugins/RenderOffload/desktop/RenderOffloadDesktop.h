#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

#include <shared/common/render/metaball_renderer.h>
#include <shared/desktop/plugin/main.h>

class RenderOffloadDesktop final : public Plugins::DesktopPlugin {
public:
    void render() override;
    void initialize_imgui(ImGuiContext *context, ImGuiMemAllocFunc *alloc_fn,
                          ImGuiMemFreeFunc *free_fn, void **user_data) override;
    std::string get_plugin_name() const override { return PLUGIN_NAME; }
    void on_websocket_message(std::string message) override;
    std::vector<std::unique_ptr<UdpPacket>>
    compute_next_packets(const std::string &scene_name) override;

    // This stream is intentionally allowed to run at 60 Hz. Render frames are
    // application-chunked below the network MTU, so they avoid IP fragmentation
    // while remaining latency-sensitive rather than using the 30 Hz video cap.
    [[nodiscard]] bool is_large_payload_plugin() const override { return false; }

private:
    using Clock = std::chrono::steady_clock;

    std::mutex mutex_;
    bool active_ = false;
    std::uint32_t session_ = 0;
    std::uint32_t sequence_ = 0;
    int target_fps_ = 60;
    std::string renderer_id_;
    Shared::Render::MetaballParams metaball_params_;
    Shared::Render::MetaballAudio metaball_audio_;
    Shared::Render::MetaballRenderer metaball_renderer_;
    float time_seconds_ = 0.0f;
    Clock::time_point last_render_{};

    bool audio_fresh_ = false;
    float audio_bass_target_ = 0.0f;
    float audio_mids_target_ = 0.0f;
    float audio_treble_target_ = 0.0f;
    float audio_balance_target_ = 0.0f;
    float audio_kick_ = 0.0f;
    std::uint64_t beat_counter_ = 0;
    std::uint64_t drop_counter_ = 0;
    std::uint64_t section_counter_ = 0;
};

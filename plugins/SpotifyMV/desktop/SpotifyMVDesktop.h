#pragma once
#include "shared/desktop/plugin/main.h"
#include "shared/desktop/VideoStreamEngine.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

class SpotifyMVDesktop final : public Plugins::DesktopPlugin {
public:
    SpotifyMVDesktop() = default;
    ~SpotifyMVDesktop() override;

    void render() override;
    void initialize_imgui(ImGuiContext*, ImGuiMemAllocFunc*, ImGuiMemFreeFunc*, void**) override;
    void pre_new_frame() override {}
    void post_init() override;
    void load_config(std::optional<const nlohmann::json>) override {}
    void save_config(nlohmann::json&) const override {}
    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    std::optional<std::unique_ptr<UdpPacket>>
        compute_next_packet(const std::string sceneName) override;

    void on_websocket_message(const std::string message) override;

private:
    [[nodiscard]] bool is_large_payload_plugin() const override { return true; }

    bool tools_available_ = false;
    std::string tools_error_msg_;
    std::mutex track_id_mutex_;
    std::string current_track_id_;

    static constexpr int kWidth  = 128;
    static constexpr int kHeight = 128;

    std::unique_ptr<Shared::VideoStreamEngine> engine_;
    std::atomic<bool> search_running_{false};
    std::thread search_thread_;

    void search_and_play(const std::string& track_id, const std::string& song,
                         const std::string& artist, const std::string& suffix,
                         bool fallback, long spotify_progress_ms, long spotify_duration_ms);

    long compute_video_seek(const std::string& url, long spotify_progress_ms,
                            long spotify_duration_ms);
};

extern "C" PLUGIN_EXPORT SpotifyMVDesktop* createSpotifyMV();
extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVDesktop*);

#pragma once
#include "shared/desktop/plugin/main.h"
#include "shared/desktop/VideoStreamEngine.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SpotifyMVDesktop final : public Plugins::DesktopPlugin {
public:
    SpotifyMVDesktop() = default;
    ~SpotifyMVDesktop() override;

    void render() override;
    void initialize_imgui(ImGuiContext*, ImGuiMemAllocFunc*, ImGuiMemFreeFunc*, void**) override;
    void pre_new_frame() override {}
    void post_init() override;
    void load_config(std::optional<const nlohmann::json>) override;
    void save_config(nlohmann::json&) const override;
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
    std::string pending_track_id_;

    static constexpr int kWidth  = 128;
    static constexpr int kHeight = 128;

    // Two engines: current_engine_ actively sends frames to the matrix;
    // pending_engine_ loads the next track in the background.
    std::unique_ptr<Shared::VideoStreamEngine> current_engine_;
    std::unique_ptr<Shared::VideoStreamEngine> pending_engine_;
    std::mutex engine_mutex_;

    std::atomic<bool> search_running_{false};
    std::thread search_thread_;

    // Crossfade state
    std::atomic<bool> crossfade_active_{false};
    std::chrono::steady_clock::time_point crossfade_start_;
    std::vector<uint8_t> old_last_frame_;
    int crossfade_duration_ms_ = 500;

    // Chunk size (seconds of video downloaded/transcoded per chunk). Smaller
    // chunks reduce the chance of a mid-playback stall on slow connections
    // (less to download before the next chunk is needed) at the cost of more
    // frequent yt-dlp/ffmpeg invocations. Configurable from the desktop UI;
    // applies to engines created after the change (not the one currently
    // playing).
    std::atomic<int> chunk_duration_sec_{20};

    // Debug counters
    std::atomic<int> total_tracks_played_{0};
    std::atomic<int> total_errors_{0};
    std::atomic<int> total_swaps_{0};

    void on_pending_first_frame();

    void search_and_play(Shared::VideoStreamEngine* engine,
                         const std::string& track_id,
                         const std::string& song,
                         const std::string& artist,
                         const std::string& suffix,
                         bool fallback,
                         long spotify_progress_ms,
                         long spotify_duration_ms);

    long compute_video_seek(const std::string& url, long spotify_progress_ms,
                            long spotify_duration_ms);
};

extern "C" PLUGIN_EXPORT SpotifyMVDesktop* createSpotifyMV();
extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVDesktop*);

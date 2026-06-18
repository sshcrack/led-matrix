#pragma once
#include "shared/desktop/macro.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Shared {

class SHARED_DESKTOP_API VideoStreamEngine {
public:
    enum class State { Idle, Downloading, Playing, Error };

    explicit VideoStreamEngine(std::filesystem::path cache_root,
                               int width = 128, int height = 128,
                               double fps = 30.0);
    ~VideoStreamEngine();

    VideoStreamEngine(const VideoStreamEngine&) = delete;
    VideoStreamEngine& operator=(const VideoStreamEngine&) = delete;

    std::string check_tools();

    void start(const std::string& url, const std::string& cache_key = "", long seek_ms = 0);
    void stop();

    void set_chunk_duration_sec(int sec) { chunk_duration_sec_ = sec; }
    // Duration of the initial fast-start clip (default 4s). Keep short so first
    // frame appears quickly; the normal chunk 0 is downloaded in parallel.
    void set_fast_chunk_duration_sec(int sec) { fast_chunk_duration_sec_ = sec; }

    std::vector<uint8_t> get_current_frame();

    bool tick();

    State get_state() const { return state_.load(); }
    std::string get_last_error() const { std::lock_guard<std::mutex> lk(error_mutex_); return last_error_; }
    std::string get_current_url() const { return current_url_; }

    // Guarded by status_cb_mutex_ — may be called from processing_thread_
    // and cleared from stop() concurrently.
    std::function<void(const std::string&)> on_status_change;

private:
    std::filesystem::path cache_root_;
    int width_, height_;
    double fps_;
    std::string current_url_;
    std::string cache_key_;

    std::atomic<State> state_{State::Idle};
    mutable std::mutex error_mutex_;
    std::mutex status_cb_mutex_;
    std::string last_error_;

    static constexpr int MAX_FIRST_CHUNK_CACHE = 10;
    int chunk_duration_sec_ = 300;
    int fast_chunk_duration_sec_ = 4;

    std::atomic<bool> running_{false};
    std::atomic<long> seek_ms_{0};
    std::thread processing_thread_;
    std::mutex prefetch_mutex_;  // guards prefetch_thread_ across stop() and processing_thread_
    std::thread prefetch_thread_;

    std::vector<uint8_t> current_frame_;
    std::mutex frame_mutex_;
    std::chrono::steady_clock::time_point last_frame_time_;

    // Command construction helpers
    std::string build_ytdlp_command(const std::filesystem::path& output_path, int start_sec, int end_sec) const;
    std::string build_ffmpeg_command(const std::filesystem::path& input_path, const std::filesystem::path& output_path) const;
    std::string build_ffmpeg_pipe_command(const std::filesystem::path& input_path) const;

    bool download_and_process_chunk(int chunk_index, bool set_error_on_fail = true);
    // Download a short clip and stream its frames directly via ffmpeg pipe,
    // so playback starts immediately without waiting for a full chunk to be encoded.
    bool play_fast_chunk(int start_sec, int duration_sec);
    std::filesystem::path chunk_mp4_path(int chunk_index) const;
    std::filesystem::path chunk_bin_path(int chunk_index) const;
    std::filesystem::path fast_chunk_mp4_path() const;
    std::string effective_cache_key() const;
    void cleanup_chunk(int chunk_index);
    void cleanup_non_first_chunks();
    void evict_oldest_first_chunks();
    void set_last_error(const std::string& msg);
    void notify_status(const std::string& s);
};

} // namespace Shared

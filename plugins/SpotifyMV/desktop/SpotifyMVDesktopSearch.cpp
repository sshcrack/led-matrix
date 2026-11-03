#include "SpotifyMVDesktop.h"
#include "YouTubeSearcher.h"
#include <spdlog/spdlog.h>

void SpotifyMVDesktop::search_and_play(Shared::VideoStreamEngine* engine,
                                       const std::string& track_id,
                                       const std::string& song,
                                       const std::string& artist,
                                       const std::string& suffix,
                                       bool fallback,
                                       long spotify_progress_ms,
                                       long spotify_duration_ms,
                                       std::uint64_t generation) {
    cancel_search_and_join();
    search_command_running_.store(true, std::memory_order_release);
    search_running_.store(true, std::memory_order_release);

    search_thread_ = std::thread([this, engine, track_id, song, artist, suffix, fallback,
                                   spotify_progress_ms, spotify_duration_ms, generation]() {
        const auto request_is_current = [this, generation] {
            return request_generation_.load(std::memory_order_relaxed) == generation;
        };

        try {
            std::string query = song + " " + artist + " " + suffix;
            std::string url = YouTubeSearcher::search(query, &search_command_running_);
            if (!request_is_current()) {
                search_running_ = false;
                return;
            }

            if (url.empty() && fallback) {
                spdlog::info("SpotifyMV: falling back to lyric video search");
                url = YouTubeSearcher::search(song + " " + artist + " lyrics", &search_command_running_);
                if (!request_is_current()) {
                    search_running_ = false;
                    return;
                }
            }

            if (url.empty()) {
                spdlog::error("SpotifyMV: no YouTube URL found for '{}'", song);
                send_websocket_message("track:error:" + track_id);
                send_websocket_message("status:error");
                search_running_ = false;
                total_errors_++;
                return;
            }

            const long seek_ms = compute_video_seek(url, spotify_progress_ms, spotify_duration_ms, &search_command_running_);
            if (!request_is_current()) {
                search_running_ = false;
                return;
            }

            // The pending engine's generation-aware callbacks are installed
            // before this search begins. VideoStreamEngine::start() preserves
            // them across its internal stop(), so the decoder can never outrun
            // first-frame registration on a cached/fast start.
            engine->start(url, track_id, seek_ms);
            if (!request_is_current()) {
                search_running_ = false;
                return;
            }
        } catch (const std::exception& e) {
            if (request_is_current()) {
                spdlog::error("SpotifyMV search exception: {}", e.what());
                send_websocket_message("track:error:" + track_id);
                send_websocket_message("status:error");
                total_errors_++;
            }
        }
        search_running_ = false;
    });
}

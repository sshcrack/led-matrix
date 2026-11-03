#include "SpotifyMVDesktop.h"
#include "SpotifyMVPacket.h"
#include <algorithm>
#include <spdlog/spdlog.h>

void SpotifyMVDesktop::on_pending_first_frame(std::uint64_t generation, const std::string& track_id) {
    std::unique_ptr<Shared::VideoStreamEngine> old_engine;
    bool do_crossfade = true;

    // Serialize promotion with track-request replacement. The captured generation
    // makes callbacks from a superseded search harmless even if the decoder
    // produces its first frame just as Spotify changes tracks.
    {
        std::unique_lock<std::mutex> lk_track(track_id_mutex_);
        if (generation != request_generation_.load(std::memory_order_relaxed)
            || pending_track_id_ != track_id)
            return;

        std::lock_guard<std::mutex> lk_engine(engine_mutex_);
        if (generation != request_generation_.load(std::memory_order_relaxed)
            || pending_track_id_ != track_id || !pending_engine_)
            return;

        total_tracks_played_++;

        if (!current_engine_) {
            // current_engine_ was reset (e.g. by a prior "stop" message), so
            // there's nothing to crossfade from — promote the prepared engine.
            current_engine_ = std::move(pending_engine_);
            current_engine_->on_first_frame_ready = nullptr;
            do_crossfade = false;
        } else {
            old_last_frame_ = current_engine_->get_current_frame();
            std::swap(current_engine_, pending_engine_);
            total_swaps_++;
            old_engine = std::move(pending_engine_);
            if (current_engine_)
                current_engine_->on_first_frame_ready = nullptr;
        }

        current_track_id_ = track_id;
        pending_track_id_.clear();
    }

    if (old_engine) {
        old_engine->on_status_change = nullptr;
        old_engine->on_first_frame_ready = nullptr;
        old_engine->stop();
    }

    send_websocket_message("track:ready:" + track_id);
    send_websocket_message("status:playing");

    if (do_crossfade && !old_last_frame_.empty()) {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        // A newer request may have arrived while the old engine was stopping.
        // Do not start a crossfade for a superseded promotion.
        if (generation != request_generation_.load(std::memory_order_relaxed))
            return;
        crossfade_start_ = std::chrono::steady_clock::now();
        crossfade_active_ = true;
    }
}

std::optional<std::unique_ptr<UdpPacket>>
SpotifyMVDesktop::compute_next_packet(const std::string sceneName) {
    if (!producer_owner_.load(std::memory_order_acquire))
        return std::nullopt;
    if (sceneName != "spotifymv")
        return std::nullopt;
    if (!tools_available_.load())
        return std::nullopt;

    std::lock_guard<std::mutex> lk(engine_mutex_);

    if (!current_engine_)
        return std::nullopt;

    auto state = current_engine_->get_state();
    bool is_playing = (state == Shared::VideoStreamEngine::State::Playing);

    // ── Try to get a fresh frame from the engine ──────────────────────
    if (is_playing) {
        bool do_tick = !crossfade_active_.load();
        if (!do_tick || current_engine_->tick()) {
            std::vector<uint8_t> frame = current_engine_->get_current_frame();
            if (!frame.empty()) {
                bool cf_active = crossfade_active_.load();
                if (cf_active) {
                    auto elapsed = std::chrono::steady_clock::now() - crossfade_start_;
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                    if (elapsed_ms >= crossfade_duration_ms_ || crossfade_duration_ms_ <= 0 || old_last_frame_.empty()) {
                        crossfade_active_ = false;
                    } else {
                        float alpha = static_cast<float>(elapsed_ms) / crossfade_duration_ms_;
                        size_t blend_pixels = (std::min)(frame.size(), old_last_frame_.size());
                        if (blend_pixels < (std::max)(frame.size(), old_last_frame_.size())) {
                            spdlog::warn("SpotifyMV crossfade: frame size mismatch ({} vs {}), blending {} pixels",
                                         frame.size(), old_last_frame_.size(), blend_pixels);
                        }
                        for (size_t i = 0; i < blend_pixels; ++i) {
                            frame[i] = static_cast<uint8_t>(
                                static_cast<float>(old_last_frame_[i]) * (1.0f - alpha) +
                                static_cast<float>(frame[i]) * alpha);
                        }
                    }
                }
                // Keep the most recent frame as freeze-frame fallback
                old_last_frame_ = frame;
                return std::make_unique<SpotifyMVPacket>(std::move(frame));
            }
        }
    }

    // ── Fallback: engine is between chunks after a transition ─────────
    // Return the last known frame instead of going dark — the matrix
    // keeps showing the old video (frozen) until playback resumes.
    if (!old_last_frame_.empty()) {
        return std::make_unique<SpotifyMVPacket>(old_last_frame_);
    }

    return std::nullopt;
}

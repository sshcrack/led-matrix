#include "SpotifyMVDesktop.h"
#include "SpotifyMVPacket.h"
#include <algorithm>
#include <spdlog/spdlog.h>

void SpotifyMVDesktop::on_pending_first_frame(std::uint64_t generation, const std::string& track_id) {
    std::unique_ptr<Shared::VideoStreamEngine> old_engine;

    // Serialize promotion with track-request replacement. The captured generation
    // makes callbacks from a superseded search harmless even if the decoder
    // produces its first frame just as Spotify changes tracks.
    {
        std::lock_guard<std::mutex> lk_track(track_id_mutex_);
        if (!track_requests_.is_pending(generation, track_id))
            return;

        std::lock_guard<std::mutex> lk_engine(engine_mutex_);
        if (!pending_engine_)
            return;

        total_tracks_played_++;

        if (current_engine_) {
            // Outgoing frame for the crossfade. Keep the existing freeze frame
            // if the old engine is between chunks and has no frame right now.
            if (auto outgoing = current_engine_->get_current_frame(); !outgoing.empty())
                old_last_frame_ = std::move(outgoing);
            old_engine = std::move(current_engine_);
            total_swaps_++;
        }
        current_engine_ = std::move(pending_engine_);
        current_engine_->on_first_frame_ready = nullptr;

        // Start the fade in the same critical section as the swap. Starting it
        // later (after stopping the old engine) let compute_next_packet()
        // overwrite old_last_frame_ with the new video first, so the "fade"
        // blended the new track into itself: a hard cut.
        if (old_engine && !old_last_frame_.empty())
            start_crossfade();
        else
            end_crossfade();

        track_requests_.promote(generation, track_id);
    }

    if (old_engine) {
        old_engine->on_status_change = nullptr;
        old_engine->on_first_frame_ready = nullptr;
        old_engine->stop();
    }

    send_websocket_message("track:ready:" + track_id);
    send_websocket_message("status:playing");
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
        const auto fade_elapsed = crossfade_elapsed();
        if (fade_elapsed.has_value() || current_engine_->tick()) {
            std::vector<uint8_t> frame = current_engine_->get_current_frame();
            if (!frame.empty()) {
                if (fade_elapsed.has_value()) {
                    const auto elapsed_ms = fade_elapsed->count();
                    if (elapsed_ms >= crossfade_duration_ms_ || crossfade_duration_ms_ <= 0 || old_last_frame_.empty()) {
                        end_crossfade();
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

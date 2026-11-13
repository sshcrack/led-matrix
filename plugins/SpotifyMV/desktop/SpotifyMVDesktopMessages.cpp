#include "SpotifyMVDesktop.h"
#include "shared/desktop/utils.h"
#include <spdlog/spdlog.h>

void SpotifyMVDesktop::on_websocket_message(const std::string message) {
    if (frames_.on_message(message))
        return;
    if (message == "producer:active") {
        producer_owner_.store(true, std::memory_order_release);
        last_tools_report_ = {};
        spdlog::info("SpotifyMV: this desktop is the active producer");
        return;
    }
    if (message == "producer:standby") {
        if (producer_owner_.exchange(false, std::memory_order_acq_rel)) {
            spdlog::info("SpotifyMV: producer ownership moved to another desktop; stopping local stream");
            stop_playback(false);
        }
        return;
    }

    if (!producer_owner_.load(std::memory_order_acquire))
        return;

    if (message == "tools:probe") {
        refresh_tools_status(true);
        report_tools_status();
        return;
    }
    if (message.starts_with("track:")) {
        refresh_tools_status(true);
        report_tools_status();
        if (!tools_available_.load()) {
            send_websocket_message("status:error");
            return;
        }

        std::string remainder = message.substr(6);

        auto colon_pos = remainder.find(':');
        if (colon_pos == std::string::npos) return;
        std::string track_id = remainder.substr(0, colon_pos);

        remainder = remainder.substr(colon_pos + 1);
        auto newline1 = remainder.find('\n');
        if (newline1 == std::string::npos) return;
        std::string song = remainder.substr(0, newline1);

        remainder = remainder.substr(newline1 + 1);
        auto newline2 = remainder.find('\n');
        if (newline2 == std::string::npos) return;
        std::string artist = remainder.substr(0, newline2);

        remainder = remainder.substr(newline2 + 1);
        auto newline3 = remainder.find('\n');
        if (newline3 == std::string::npos) return;
        std::string suffix = remainder.substr(0, newline3);

        remainder = remainder.substr(newline3 + 1);
        auto newline4 = remainder.find('\n');
        std::string fallback_str;
        std::string progress_ms_str = "0";
        std::string duration_ms_str = "0";
        if (newline4 != std::string::npos) {
            fallback_str = remainder.substr(0, newline4);
            remainder = remainder.substr(newline4 + 1);
            auto newline5 = remainder.find('\n');
            if (newline5 != std::string::npos) {
                progress_ms_str = remainder.substr(0, newline5);
                duration_ms_str = remainder.substr(newline5 + 1);
            } else {
                progress_ms_str = remainder;
            }
        } else {
            fallback_str = remainder;
        }
        bool fallback = (fallback_str == "true");

        long progress_ms = 0, duration_ms = 0;
        try {
            progress_ms = std::stol(progress_ms_str);
            duration_ms = std::stol(duration_ms_str);
        } catch (...) {
            spdlog::warn("SpotifyMV: invalid progress/duration values, using 0");
        }

        // ── Request state transition ──────────────────────────────────────
        // track_requests_ is serialized by track_id_mutex_. on_pending_first_frame
        // takes the same mutex before engine_mutex_, so a superseded callback
        // cannot promote the wrong track.
        SpotifyMVTrackRequests::Decision decision;
        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            const bool is_known = (track_requests_.current() && track_requests_.current()->track_id == track_id)
                || (track_requests_.pending() && track_requests_.pending()->track_id == track_id);
            if (!is_known && song.empty() && artist.empty()) {
                spdlog::warn("SpotifyMV: empty song/artist, skipping search");
                send_websocket_message("status:error");
                return;
            }
            decision = track_requests_.request(track_id);
        }

        switch (decision.outcome) {
        case SpotifyMVTrackRequests::Outcome::AlreadyCurrent:
        case SpotifyMVTrackRequests::Outcome::AlreadyPending:
            return;
        case SpotifyMVTrackRequests::Outcome::ReturnToCurrent: {
            // Went back to the currently playing track while a different
            // track was loading — cancel the pending engine and keep playing
            // the current one as-is. No plugin mutex is held while joining:
            // the search worker can finish callbacks without lock inversion.
            spdlog::info("SpotifyMV: went back to current track, cancelling pending");
            cancel_search_and_join();
            std::unique_ptr<Shared::VideoStreamEngine> engine_to_cancel;
            {
                std::lock_guard<std::mutex> lk_eng(engine_mutex_);
                engine_to_cancel = std::move(pending_engine_);
            }
            if (engine_to_cancel) {
                engine_to_cancel->on_status_change = nullptr;
                engine_to_cancel->on_first_frame_ready = nullptr;
                engine_to_cancel->stop();
            }
            send_websocket_message("status:playing");
            return;
        }
        case SpotifyMVTrackRequests::Outcome::NewRequest:
            break;
        }
        const std::uint64_t generation = decision.generation;

        // ── Cancel any existing pending engine ───────────────────────────
        std::unique_ptr<Shared::VideoStreamEngine> to_stop;
        {
            std::lock_guard<std::mutex> lk_eng(engine_mutex_);
            if (pending_engine_) {
                pending_engine_->on_status_change = nullptr;
                pending_engine_->on_first_frame_ready = nullptr;
                to_stop = std::move(pending_engine_);
            }
        }
        if (to_stop) {
            cancel_search_and_join();
            to_stop->stop();
        }

        // ── Create new pending engine ────────────────────────────────────
        auto cacheRoot = get_data_dir() / "cache" / "spotifymv";
        auto new_engine = std::make_unique<Shared::VideoStreamEngine>(cacheRoot, kWidth, kHeight);
        new_engine->set_chunk_duration_sec(chunk_duration_sec_.load());
        // NOTE: on_status_change must be wired here too, not just in
        // post_init(). Engines are swapped (not copied) on
        // on_pending_first_frame(), so the object that ends up as
        // current_engine_ after a swap is *this* object — without this it
        // would never forward status (downloading/error/etc.) once it
        // becomes current. The gate is the request state's status
        // generation: this engine's status is forwarded while it is the
        // pending request, and again once it is current with nothing pending
        // (including after the user returned to it from a skipped track).
        new_engine->on_status_change = [this, generation](const std::string& status) {
            if (track_requests_.status_generation() != generation)
                return;
            spdlog::info("Status change " + status);
            send_websocket_message("status:" + status);
        };
        new_engine->on_first_frame_ready = [this, generation, track_id]() {
            on_pending_first_frame(generation, track_id);
        };

        {
            std::lock_guard<std::mutex> lk_eng(engine_mutex_);
            pending_engine_ = std::move(new_engine);
        }

        send_websocket_message("track:preparing:" + track_id);
        send_websocket_message("status:pending");

        search_and_play(pending_engine_.get(), track_id, song, artist,
                        suffix, fallback, progress_ms, duration_ms, generation);
        return;
    }

    if (message == "stop") {
        stop_playback(true);
        return;
    }
}

#include "SpotifyMVDesktop.h"
#include "SpotifyMVPacket.h"
#include "YouTubeSearcher.h"
#include "shared/desktop/utils.h"
#include <fmt/format.h>
#include <imgui.h>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {

const char* engine_state_label(Shared::VideoStreamEngine::State s) {
    switch (s) {
    case Shared::VideoStreamEngine::State::Idle:        return "Idle";
    case Shared::VideoStreamEngine::State::Downloading: return "Downloading";
    case Shared::VideoStreamEngine::State::Playing:     return "Playing";
    case Shared::VideoStreamEngine::State::Error:       return "Error";
    }
    return "Unknown";
}

ImVec4 engine_state_color(Shared::VideoStreamEngine::State s) {
    switch (s) {
    case Shared::VideoStreamEngine::State::Idle:        return {0.5f, 0.5f, 0.5f, 1};
    case Shared::VideoStreamEngine::State::Downloading: return {1, 1, 0, 1};
    case Shared::VideoStreamEngine::State::Playing:     return {0, 1, 0, 1};
    case Shared::VideoStreamEngine::State::Error:       return {1, 0, 0, 1};
    }
    return {1, 1, 1, 1};
}

void draw_engine_status(Shared::VideoStreamEngine::State state,
                         const std::string& error) {
    ImGui::TextColored(engine_state_color(state), "State: %s",
                       engine_state_label(state));
    ImGui::SameLine();
    ImGui::ProgressBar(0, ImVec2(-FLT_MIN, 0), "");
    if (state == Shared::VideoStreamEngine::State::Error && !error.empty())
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", error.c_str());
}

} // anonymous namespace

extern "C" PLUGIN_EXPORT SpotifyMVDesktop* createSpotifyMV() {
    return new SpotifyMVDesktop();
}

extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVDesktop* c) { delete c; }

SpotifyMVDesktop::~SpotifyMVDesktop() {
    if (search_thread_.joinable()) search_thread_.join();
    if (pending_engine_) {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        pending_engine_->stop();
        // Clear AFTER stop() — stop() restores the captured callback, so we
        // must null it out afterward to prevent ~VideoStreamEngine() from
        // holding a dangling capture (the lambda's `this` becomes invalid as
        // member destructors unwind).
        pending_engine_->on_status_change = nullptr;
        pending_engine_->on_first_frame_ready = nullptr;
    }
    if (current_engine_) {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        current_engine_->stop();
        current_engine_->on_status_change = nullptr;
        current_engine_->on_first_frame_ready = nullptr;
    }
}

void SpotifyMVDesktop::post_init() {
    auto cacheRoot = get_data_dir() / "cache" / "spotifymv";
    std::filesystem::create_directories(cacheRoot);
    current_engine_ = std::make_unique<Shared::VideoStreamEngine>(cacheRoot, kWidth, kHeight);
    auto err = current_engine_->check_tools();
    tools_available_ = err.empty();
    if (!err.empty()) {
        tools_error_msg_ = err;
        spdlog::error(tools_error_msg_);
    }
    current_engine_->on_status_change = [this](const std::string& s) {
        spdlog::info("Status change " + s);
        send_websocket_message("status:" + s);
    };
    current_engine_->set_chunk_duration_sec(chunk_duration_sec_.load());
}

void SpotifyMVDesktop::load_config(std::optional<const nlohmann::json> config) {
    if (config.has_value()) {
        const auto& cfg = config.value();
        if (cfg.contains("crossfade_duration_ms"))
            crossfade_duration_ms_ = cfg["crossfade_duration_ms"].get<int>();
        if (cfg.contains("chunk_duration_sec"))
            chunk_duration_sec_.store(cfg["chunk_duration_sec"].get<int>());
    }
}

void SpotifyMVDesktop::save_config(nlohmann::json& config) const {
    config["crossfade_duration_ms"] = crossfade_duration_ms_;
    config["chunk_duration_sec"] = chunk_duration_sec_.load();
}

void SpotifyMVDesktop::initialize_imgui(ImGuiContext* ctx,
                                        ImGuiMemAllocFunc* alloc_fn,
                                        ImGuiMemFreeFunc* free_fn,
                                        void** user_data) {
    ImGui::SetCurrentContext(ctx);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void SpotifyMVDesktop::render() {
    if (!tools_available_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", tools_error_msg_.c_str());
        return;
    }

    ImGui::SliderInt("Crossfade (ms)", &crossfade_duration_ms_, 0, 2000);

    {
        int chunk_sec = chunk_duration_sec_.load();
        if (ImGui::SliderInt("Chunk size (sec)", &chunk_sec, 5, 120)) {
            chunk_duration_sec_.store(chunk_sec);
        }
        ImGui::TextDisabled("Smaller chunks buffer less but stall less on slow connections.\n"
                             "Applies to the next track loaded, not the one currently playing.");
    }

    // ── Snapshot engine state under lock ────────────────────────────────
    std::string cur_url;
    Shared::VideoStreamEngine::State cur_state = Shared::VideoStreamEngine::State::Idle;
    std::string cur_error;
    bool has_pending = false;
    std::string pend_url;
    Shared::VideoStreamEngine::State pend_state = Shared::VideoStreamEngine::State::Idle;
    std::string pend_error;
    {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        if (current_engine_) {
            cur_url = current_engine_->get_current_url();
            cur_state = current_engine_->get_state();
            cur_error = current_engine_->get_last_error();
        }
        if (pending_engine_) {
            has_pending = true;
            pend_url = pending_engine_->get_current_url();
            pend_state = pending_engine_->get_state();
            pend_error = pending_engine_->get_last_error();
        }
    }

    std::string cur_track;
    std::string pend_track;
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        cur_track = current_track_id_;
        pend_track = pending_track_id_;
    }

    // ── Overall status bar ──────────────────────────────────────────────
    {
        ImVec4 barColor = {0.3f, 0.3f, 0.3f, 1};
        const char* barLabel = "Idle";
        if (cur_state == Shared::VideoStreamEngine::State::Downloading) {
            barColor = {1, 1, 0, 1}; barLabel = "Downloading";
        } else if (cur_state == Shared::VideoStreamEngine::State::Playing) {
            barColor = {0, 1, 0, 1}; barLabel = "Playing";
        } else if (cur_state == Shared::VideoStreamEngine::State::Error) {
            barColor = {1, 0, 0, 1}; barLabel = "Error";
        }
        if (crossfade_active_.load()) {
            barColor = {0.5f, 0.5f, 1, 1}; barLabel = "Crossfading";
        } else if (search_running_.load() && cur_state != Shared::VideoStreamEngine::State::Playing) {
            barColor = {0, 0.84f, 0.38f, 1}; barLabel = "Searching";
        }
        ImGui::PushStyleColor(ImGuiCol_Button, barColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, barColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, barColor);
        ImGui::Button(barLabel, ImVec2(-FLT_MIN, 28));
        ImGui::PopStyleColor(3);
    }

    // ── Current track info ──────────────────────────────────────────────
    ImGui::Text("Track: %s", cur_track.empty() ? "None" : cur_track.c_str());
    if (!pend_track.empty())
        ImGui::Text("Next:  %s", pend_track.c_str());
    if (!cur_url.empty())
        ImGui::Text("URL:   %s", cur_url.c_str());

    // ── Search status ───────────────────────────────────────────────────
    if (search_running_.load())
        ImGui::TextColored(ImVec4(0, 0.84f, 0.38f, 1), "Searching YouTube...");

    // ── Crossfade progress ──────────────────────────────────────────────
    if (crossfade_active_.load()) {
        auto elapsed = std::chrono::steady_clock::now() - crossfade_start_;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        float ratio = static_cast<float>(elapsed_ms) / crossfade_duration_ms_;
        float progress = ratio > 1.0f ? 1.0f : ratio;
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0), "Crossfade");
    }

    // ── Engine status cards ─────────────────────────────────────────────
    if (ImGui::BeginTable("##engines", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Current Engine");
        draw_engine_status(cur_state, cur_error);
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Pending Engine");
        if (has_pending)
            draw_engine_status(pend_state, pend_error);
        else
            ImGui::TextDisabled("None");
        ImGui::EndTable();
    }

    // ── Error display ────────────────────────────────────────────────────
    if (cur_state == Shared::VideoStreamEngine::State::Error && !cur_error.empty())
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", cur_error.c_str());
    if (has_pending && pend_state == Shared::VideoStreamEngine::State::Error && !pend_error.empty())
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Pending Error: %s", pend_error.c_str());

    // ── Debug section ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Debug Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Total tracks played: %d", total_tracks_played_.load());
        ImGui::Text("Total errors:       %d", total_errors_.load());
        ImGui::Text("Engine swaps:       %d", total_swaps_.load());
        ImGui::Text("Search running:     %s", search_running_.load() ? "Yes" : "No");
        ImGui::Text("Crossfade active:   %s", crossfade_active_.load() ? "Yes" : "No");
        ImGui::Separator();
        ImGui::Text("Current engine:     %s", current_engine_ ? "Valid" : "NULL");
        ImGui::Text("Pending engine:     %s", pending_engine_ ? "Valid" : "NULL");
        if (has_pending)
            ImGui::Text("Pending URL:  %s", pend_url.empty() ? "None" : pend_url.c_str());
    }
}

void SpotifyMVDesktop::on_pending_first_frame() {
    std::unique_ptr<Shared::VideoStreamEngine> old_engine;
    bool do_crossfade = true;
    {
        std::lock_guard<std::mutex> lk(engine_mutex_);

        if (!pending_engine_)
            return;

        total_tracks_played_++;

        if (!current_engine_) {
            // current_engine_ was reset (e.g. by a prior "stop" message), so
            // there's nothing to crossfade from — just promote the pending
            // engine directly. Without this, the swap below would silently
            // no-op forever: the pending engine keeps decoding frames in the
            // background, but compute_next_packet() only ever reads
            // current_engine_ (null), so no frames are ever sent and the UI
            // is stuck showing "Current track: None" with a pending track
            // that never finishes loading.
            current_engine_ = std::move(pending_engine_);
            current_engine_->on_first_frame_ready = nullptr;
            do_crossfade = false;
        } else {
            // Capture the old engine's last frame before the swap
            old_last_frame_ = current_engine_->get_current_frame();

            // Swap engines
            std::swap(current_engine_, pending_engine_);
            total_swaps_++;

            // Take ownership of the old engine to stop outside the lock,
            // avoiding deadlock: processing thread holds status_cb_mutex_ and
            // calls this callback which would acquire engine_mutex_ then
            // status_cb_mutex_ again via stop(); on_websocket_message holds
            // engine_mutex_ then status_cb_mutex_.
            old_engine = std::move(pending_engine_);
            if (current_engine_)
                current_engine_->on_first_frame_ready = nullptr;
        }
    }

    if (old_engine) {
        old_engine->on_status_change = nullptr;
        old_engine->on_first_frame_ready = nullptr;
        old_engine->stop();
    }

    // Update track IDs
    {
        std::lock_guard<std::mutex> lk_track(track_id_mutex_);
        current_track_id_ = pending_track_id_;
        pending_track_id_.clear();
    }

    send_websocket_message("status:playing");

    // Start crossfade (only if we actually have an old frame captured to
    // blend from — not the case when current_engine_ was null, i.e. this is
    // effectively a fresh start rather than a transition).
    if (do_crossfade && !old_last_frame_.empty()) {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        crossfade_start_ = std::chrono::steady_clock::now();
        crossfade_active_ = true;
    }
}

std::optional<std::unique_ptr<UdpPacket>>
SpotifyMVDesktop::compute_next_packet(const std::string sceneName) {
    if (sceneName != "spotifymv")
        return std::nullopt;
    if (!tools_available_)
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

void SpotifyMVDesktop::on_websocket_message(const std::string message) {
    if (message.starts_with("track:")) {
        std::string remainder = message.substr(6);

        auto colon_pos = remainder.find(':');
        if (colon_pos == std::string::npos) return;
        std::string track_id = remainder.substr(0, colon_pos);

        // ── Dedup ─────────────────────────────────────────────────────────
        // Lock ordering: track_id_mutex_ is released BEFORE engine_mutex_ is
        // taken, to avoid deadlock with on_pending_first_frame (engine → track_id).
        std::unique_ptr<Shared::VideoStreamEngine> engine_to_cancel;
        bool went_back = false;
        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            if (track_id == current_track_id_) {
                if (!pending_track_id_.empty() && track_id != pending_track_id_) {
                    // Went back to the currently playing track while a different
                    // track was loading — cancel the pending engine and keep
                    // playing the current one as-is.
                    // NOTE: we do NOT re-seek the current engine, so there will
                    // be a loose sync with Spotify progress (~1-4s window).
                    spdlog::info("SpotifyMV: went back to current track, cancelling pending");
                    pending_track_id_.clear();
                    // Join search thread before destroying the engine it may be using
                    if (search_thread_.joinable()) search_thread_.join();
                    engine_to_cancel = std::move(pending_engine_);
                    went_back = true;
                }
                return;
            }
            if (track_id == pending_track_id_)
                return;
        }
        // track_id_mutex_ released — now safe to acquire engine_mutex_
        if (engine_to_cancel) {
            engine_to_cancel->on_status_change = nullptr;
            engine_to_cancel->on_first_frame_ready = nullptr;
            engine_to_cancel->stop();
        }
        if (went_back) {
            send_websocket_message("status:playing");
            return;
        }

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

        if (song.empty() && artist.empty()) {
            spdlog::warn("SpotifyMV: empty song/artist, skipping search");
            send_websocket_message("status:error");
            return;
        }

        long progress_ms = 0, duration_ms = 0;
        try {
            progress_ms = std::stol(progress_ms_str);
            duration_ms = std::stol(duration_ms_str);
        } catch (...) {
            spdlog::warn("SpotifyMV: invalid progress/duration values, using 0");
        }

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
            if (search_thread_.joinable()) search_thread_.join();
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
        // becomes current, e.g. errors after the first track change would
        // go unreported and the matrix would just freeze on the old frame.
        new_engine->on_status_change = [this](const std::string& s) {
            spdlog::info("Status change " + s);
            send_websocket_message("status:" + s);
        };
        new_engine->on_first_frame_ready = [this]() {
            on_pending_first_frame();
        };

        {
            std::lock_guard<std::mutex> lk_eng(engine_mutex_);
            pending_engine_ = std::move(new_engine);
        }

        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            pending_track_id_ = track_id;
        }

        send_websocket_message("status:pending");

        search_and_play(pending_engine_.get(), track_id, song, artist,
                        suffix, fallback, progress_ms, duration_ms);
        return;
    }

        if (message == "stop") {
        if (search_thread_.joinable()) search_thread_.join();

        {
            std::lock_guard<std::mutex> lk_eng(engine_mutex_);
            if (pending_engine_) {
                pending_engine_->on_status_change = nullptr;
                pending_engine_->on_first_frame_ready = nullptr;
                pending_engine_->stop();
                pending_engine_.reset();
            }
            if (current_engine_) {
                current_engine_->on_status_change = nullptr;
                current_engine_->on_first_frame_ready = nullptr;
                current_engine_->stop();
                current_engine_.reset();
            }
            crossfade_active_ = false;
            old_last_frame_.clear();
        }
        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            current_track_id_ = "";
            pending_track_id_ = "";
        }
        send_websocket_message("status:idle");
        return;
    }
}

void SpotifyMVDesktop::search_and_play(Shared::VideoStreamEngine* engine,
                                       const std::string& track_id,
                                       const std::string& song,
                                       const std::string& artist,
                                       const std::string& suffix,
                                       bool fallback,
                                       long spotify_progress_ms,
                                       long spotify_duration_ms) {
    if (search_thread_.joinable()) search_thread_.join();

    search_running_ = true;

    search_thread_ = std::thread([this, engine, track_id, song, artist, suffix, fallback,
                                   spotify_progress_ms, spotify_duration_ms]() {
        try {
            std::string query = song + " " + artist + " " + suffix;
            std::string url = YouTubeSearcher::search(query);

            if (url.empty() && fallback) {
                spdlog::info("SpotifyMV: falling back to lyric video search");
                url = YouTubeSearcher::search(song + " " + artist + " lyrics");
            }

            if (url.empty()) {
                spdlog::error("SpotifyMV: no YouTube URL found for '{}'", song);
                send_websocket_message("status:error");
                search_running_ = false;
                total_errors_++;
                return;
            }

            long seek_ms = compute_video_seek(url, spotify_progress_ms, spotify_duration_ms);
            engine->start(url, track_id, seek_ms);
            // Re-set callbacks that start() cleared via its internal stop() call
            engine->on_status_change = [this](const std::string& s) {
                spdlog::info("Status change " + s);
                send_websocket_message("status:" + s);
            };
            engine->on_first_frame_ready = [this]() {
                on_pending_first_frame();
            };
        } catch (const std::exception& e) {
            spdlog::error("SpotifyMV search exception: {}", e.what());
            send_websocket_message("status:error");
            total_errors_++;
        }
        search_running_ = false;
    });
}

long SpotifyMVDesktop::compute_video_seek(const std::string& url,
                                           long spotify_progress_ms,
                                           long spotify_duration_ms) {
    if (spotify_duration_ms <= 0)
        return spotify_progress_ms;

    std::string video_id;
    auto vpos = url.find("v=");
    if (vpos != std::string::npos) {
        video_id = url.substr(vpos + 2);
        auto amp = video_id.find('&');
        if (amp != std::string::npos) video_id = video_id.substr(0, amp);
    } else {
        auto last_slash = url.rfind('/');
        if (last_slash != std::string::npos) {
            video_id = url.substr(last_slash + 1);
            auto qmark = video_id.find('?');
            if (qmark != std::string::npos) video_id = video_id.substr(0, qmark);
        }
    }
    if (video_id.empty()) {
        spdlog::warn("SpotifyMV: could not extract video ID from URL");
        return spotify_progress_ms;
    }

    double video_duration = 0;
    std::string durCmd = "yt-dlp --no-warnings --print duration \"" + url + "\" 2>"
#ifdef _WIN32
                         "nul";
#else
                         "/dev/null";
#endif
    std::string durOut = run_command_and_get_output(durCmd);
    if (!durOut.empty()) {
        try { video_duration = std::stod(durOut); } catch (...) {}
    }
    if (video_duration <= 0) {
        spdlog::warn("SpotifyMV: could not get video duration, falling back to raw seek");
        return spotify_progress_ms;
    }

    double intro_end = 0;
    double outro_start = video_duration;

    {
        auto response = cpr::Get(
            cpr::Url{"https://sponsor.ajay.app/api/skipSegments"},
            cpr::Parameters{
                {"videoID", video_id},
                {"categories", R"(["intro","outro","music_offtopic"])"}
            },
            cpr::Timeout{3000}
        );

        if (response.status_code == 200 && !response.text.empty()) {
            try {
                auto segments = nlohmann::json::parse(response.text);
                for (auto& seg : segments) {
                    if (!seg.contains("segment") || !seg["segment"].is_array()
                        || seg["segment"].size() < 2)
                        continue;
                    double start = seg["segment"][0].get<double>();
                    double end = seg["segment"][1].get<double>();
                    std::string cat = seg.value("category", "");

                    if (cat == "intro" && end > intro_end)
                        intro_end = end;
                    if (cat == "outro" && start < outro_start)
                        outro_start = start;
                    if (cat == "music_offtopic") {
                        if (end > intro_end && end < video_duration / 2)
                            intro_end = end;
                        if (start < outro_start && start > video_duration / 2)
                            outro_start = start;
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("SpotifyMV: failed to parse SponsorBlock JSON: {}", e.what());
            }
        }
    }

    double spotify_dur_sec = spotify_duration_ms / 1000.0;
    if (intro_end == 0 && outro_start >= video_duration && video_duration > spotify_dur_sec) {
        double diff = video_duration - spotify_dur_sec;
        intro_end = diff * 0.35;
        outro_start = video_duration - diff * 0.65;
        spdlog::info("SpotifyMV: no SponsorBlock data, using duration heuristic (intro={:.1f}s, outro={:.1f}s)",
                     intro_end, video_duration - outro_start);
    }

    double music_duration = outro_start - intro_end;
    if (music_duration <= 0) {
        spdlog::warn("SpotifyMV: invalid music duration ({:.1f}s), falling back to raw seek", music_duration);
        return spotify_progress_ms;
    }

    double ratio = static_cast<double>(spotify_progress_ms) / spotify_duration_ms;
    double video_seek_sec = intro_end + ratio * music_duration;
    if (video_seek_sec < 0) video_seek_sec = 0;
    if (video_seek_sec > video_duration) video_seek_sec = video_duration;

    long seek_ms = static_cast<long>(video_seek_sec * 1000.0);
    spdlog::info("SpotifyMV: {}ms in song \u2192 {}ms in video (intro={:.1f}s, outro={:.1f}s, ratio={:.3f})",
                 spotify_progress_ms, seek_ms, intro_end, outro_start, ratio);
    return seek_ms;
}

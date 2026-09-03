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

void draw_error_text(const char* prefix, const std::string& error) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
    ImGui::TextWrapped("%s%s", prefix, error.c_str());
    ImGui::PopStyleColor();
}

void draw_engine_status(Shared::VideoStreamEngine::State state,
                         const std::string& error) {
    ImGui::TextColored(engine_state_color(state), "State: %s",
                       engine_state_label(state));
    ImGui::SameLine();
    ImGui::ProgressBar(0, ImVec2(-FLT_MIN, 0), "");
    if (state == Shared::VideoStreamEngine::State::Error && !error.empty())
        draw_error_text("", error);
}

} // anonymous namespace

REGISTER_PLUGIN(SpotifyMV, SpotifyMVDesktop)

SpotifyMVDesktop::~SpotifyMVDesktop() {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        request_generation_.fetch_add(1, std::memory_order_relaxed);
        pending_track_id_.clear();
    }
    cancel_search_and_join();
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
    refresh_tools_status(true);
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

bool SpotifyMVDesktop::refresh_tools_status(bool force) {
    const auto configured_path = Config::ConfigManager::instance()->getGeneralConfig().getYtDlpPath();
    {
        std::lock_guard<std::mutex> lock(tools_status_mutex_);
        if (!force && configured_path == last_checked_ytdlp_path_)
            return false;
        last_checked_ytdlp_path_ = configured_path;
    }

    const auto error = check_video_tools_available();
    tools_available_.store(error.empty());
    {
        std::lock_guard<std::mutex> lock(tools_status_mutex_);
        tools_error_msg_ = error;
    }
    if (!error.empty())
        spdlog::error("SpotifyMV desktop tools unavailable: {}", error);
    return true;
}

void SpotifyMVDesktop::report_tools_status() {
    send_websocket_message(tools_available_.load() ? "tools:ready" : "tools:error");
    last_tools_report_ = std::chrono::steady_clock::now();
}

void SpotifyMVDesktop::pre_new_frame() {
    if (!producer_owner_.load(std::memory_order_acquire))
        return;
    const bool changed = refresh_tools_status();
    const auto now = std::chrono::steady_clock::now();
    if (changed || last_tools_report_ == std::chrono::steady_clock::time_point{}
        || now - last_tools_report_ >= std::chrono::seconds(2)) {
        report_tools_status();
    }
}

void SpotifyMVDesktop::render() {
    ImGui::Text("Producer role: %s", producer_owner_.load(std::memory_order_acquire) ? "Active" : "Standby");
    if (!tools_available_.load()) {
        std::string error;
        {
            std::lock_guard<std::mutex> lock(tools_status_mutex_);
            error = tools_error_msg_;
        }
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", error.c_str());
        ImGui::TextDisabled("Configure the shared yt-dlp binary under External Tools.");
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
    if (search_running_.load()) {
        const char* preparation_label =
            cur_state == Shared::VideoStreamEngine::State::Playing
                ? "Preparing upcoming Spotify video in background..."
                : "Searching YouTube for Spotify video...";
        ImGui::TextColored(ImVec4(0, 0.84f, 0.38f, 1), "%s", preparation_label);
    }

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
        draw_error_text("Error: ", cur_error);
    if (has_pending && pend_state == Shared::VideoStreamEngine::State::Error && !pend_error.empty())
        draw_error_text("Pending Error: ", pend_error);

    // ── Debug section ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Debug Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Total tracks played: %d", total_tracks_played_.load());
        ImGui::Text("Total errors:       %d", total_errors_.load());
        ImGui::Text("Engine swaps:       %d", total_swaps_.load());
        ImGui::Text("Background prepare: %s", search_running_.load() ? "Yes" : "No");
        ImGui::Text("Crossfade active:   %s", crossfade_active_.load() ? "Yes" : "No");
        ImGui::Separator();
        ImGui::Text("Current engine:     %s", current_engine_ ? "Valid" : "NULL");
        ImGui::Text("Pending engine:     %s", pending_engine_ ? "Valid" : "NULL");
        if (has_pending)
            ImGui::Text("Pending URL:  %s", pend_url.empty() ? "None" : pend_url.c_str());
    }
}

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

void SpotifyMVDesktop::on_websocket_message(const std::string message) {
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

        // ── Dedup ─────────────────────────────────────────────────────────
        // Request identity is serialized by track_id_mutex_. on_pending_first_frame
        // takes the same mutex before engine_mutex_, so a superseded callback
        // cannot promote the wrong track.
        std::unique_ptr<Shared::VideoStreamEngine> engine_to_cancel;
        bool went_back = false;
        bool already_current = false;
        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            if (track_id == current_track_id_) {
                if (!pending_track_id_.empty() && track_id != pending_track_id_) {
                    // Went back to the currently playing track while a different
                    // track was loading — cancel the pending engine and keep
                    // playing the current one as-is.
                    spdlog::info("SpotifyMV: went back to current track, cancelling pending");
                    request_generation_.fetch_add(1, std::memory_order_relaxed);
                    pending_track_id_.clear();
                    went_back = true;
                } else {
                    already_current = true;
                }
            } else if (track_id == pending_track_id_) {
                return;
            }
        }
        if (already_current)
            return;
        if (went_back) {
            // No plugin mutex is held while joining: the search worker can finish
            // callbacks without lock inversion. Move/stop its engine only after
            // the worker no longer references it.
            cancel_search_and_join();
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

        std::uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lk(track_id_mutex_);
            generation = request_generation_.fetch_add(1, std::memory_order_relaxed) + 1;
            pending_track_id_ = track_id;
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
        // becomes current, e.g. errors after the first track change would
        // go unreported and the matrix would just freeze on the old frame.
        new_engine->on_status_change = [this, generation](const std::string& status) {
            if (request_generation_.load(std::memory_order_relaxed) != generation)
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

void SpotifyMVDesktop::stop_playback(const bool report_status) {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        request_generation_.fetch_add(1, std::memory_order_relaxed);
        current_track_id_.clear();
        pending_track_id_.clear();
    }
    cancel_search_and_join();

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

    if (report_status) {
        send_websocket_message("track:idle");
        send_websocket_message("status:idle");
    }
}

void SpotifyMVDesktop::cancel_search_and_join() {
    search_command_running_.store(false, std::memory_order_release);
    if (search_thread_.joinable())
        search_thread_.join();
    search_running_.store(false, std::memory_order_release);
}

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

long SpotifyMVDesktop::compute_video_seek(const std::string& url,
                                           long spotify_progress_ms,
                                           long spotify_duration_ms,
                                           const std::atomic<bool>* running) {
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
    const std::string durCmd = get_ytdlp_network_command()
        + " --no-warnings --print duration \"" + url + "\"";
    const auto duration_result = run_command_capture(durCmd, running, 4096);
    if (duration_result.exit_code == -2)
        return spotify_progress_ms;
    if (duration_result.exit_code == 0 && !duration_result.output.empty()) {
        try { video_duration = std::stod(duration_result.output); } catch (...) {}
    }
    if (video_duration <= 0) {
        spdlog::warn("SpotifyMV: could not get video duration, falling back to raw seek");
        return spotify_progress_ms;
    }

    double intro_end = 0;
    double outro_start = video_duration;

    if (running && !running->load(std::memory_order_acquire))
        return spotify_progress_ms;

    {
        auto response = cpr::Get(
            cpr::Url{"https://sponsor.ajay.app/api/skipSegments"},
            cpr::Parameters{
                {"videoID", video_id},
                {"categories", R"(["intro","outro","music_offtopic"])"}
            },
            cpr::Timeout{1200}
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

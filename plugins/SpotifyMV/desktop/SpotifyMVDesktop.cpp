#include "SpotifyMVDesktop.h"
#include "shared/desktop/utils.h"
#include <fmt/format.h>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

REGISTER_PLUGIN(SpotifyMV, SpotifyMVDesktop)

SpotifyMVDesktop::~SpotifyMVDesktop() {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        track_requests_.clear();
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

void SpotifyMVDesktop::stop_playback(const bool report_status) {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        track_requests_.clear();
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
        end_crossfade();
        old_last_frame_.clear();
    }

    if (report_status) {
        send_websocket_message("track:idle");
        send_websocket_message("status:idle");
    }
}

bool SpotifyMVDesktop::request_is_pending(const std::uint64_t generation) {
    std::lock_guard<std::mutex> lk(track_id_mutex_);
    return track_requests_.is_pending(generation);
}

void SpotifyMVDesktop::start_crossfade() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    crossfade_started_ns_.store(
        std::max<std::int64_t>(1, std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()),
        std::memory_order_release);
}

std::optional<std::chrono::milliseconds> SpotifyMVDesktop::crossfade_elapsed() const {
    const auto started = crossfade_started_ns_.load(std::memory_order_acquire);
    if (started == 0) return std::nullopt;
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - std::chrono::nanoseconds(started));
}

void SpotifyMVDesktop::cancel_search_and_join() {
    search_command_running_.store(false, std::memory_order_release);
    if (search_thread_.joinable())
        search_thread_.join();
    search_running_.store(false, std::memory_order_release);
}

#include "SpotifyMVDesktop.h"

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <nlohmann/json.hpp>

#include "shared/desktop/utils.h"

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
        pending_engine_->clear_callbacks();
    }
    if (current_engine_) {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        current_engine_->stop();
        current_engine_->clear_callbacks();
    }
}

void SpotifyMVDesktop::post_init() {
    auto cacheRoot = get_data_dir() / "cache" / "spotifymv";
    std::filesystem::create_directories(cacheRoot);
    current_engine_ = std::make_unique<Shared::VideoStreamEngine>(cacheRoot, kWidth, kHeight);
    refresh_tools_status(true);
    current_engine_->set_on_status_change([this](const std::string& s) {
        spdlog::info("Status change " + s);
        send_websocket_message("status:" + s);
    });
    current_engine_->set_chunk_duration_sec(chunk_duration_sec_.load());
}

void SpotifyMVDesktop::load_config(std::optional<const nlohmann::json> config) {
    if (config.has_value()) {
        const auto& cfg = config.value();
        if (cfg.contains("crossfade_duration_ms"))
            crossfade_duration_ms_.store(cfg["crossfade_duration_ms"].get<int>());
        if (cfg.contains("chunk_duration_sec"))
            chunk_duration_sec_.store(cfg["chunk_duration_sec"].get<int>());
    }
}

void SpotifyMVDesktop::save_config(nlohmann::json& config) const {
    config["crossfade_duration_ms"] = crossfade_duration_ms_.load();
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
    last_tools_report_ns_.store(steady_now_ns(), std::memory_order_relaxed);
}

void SpotifyMVDesktop::pre_new_frame() {
    if (!producer_owner_.load(std::memory_order_acquire))
        return;
    const bool changed = refresh_tools_status();
    const auto now = steady_now_ns();
    const auto last_report = last_tools_report_ns_.load(std::memory_order_relaxed);
    if (changed || last_report == 0 ||
        now - last_report >= std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2)).count()) {
        report_tools_status();
    }
}

void SpotifyMVDesktop::stop_playback(const bool report_status) {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        track_requests_.clear();
    }
    cancel_search_and_join();

    // Detach under the lock, stop outside it: stop() joins the engine threads
    // (and may wait for ffmpeg to die), which must not stall
    // compute_next_packet() on the UDP thread.
    std::unique_ptr<Shared::VideoStreamEngine> pending;
    std::unique_ptr<Shared::VideoStreamEngine> current;
    {
        std::lock_guard<std::mutex> lk_eng(engine_mutex_);
        pending = std::move(pending_engine_);
        current = std::move(current_engine_);
        end_crossfade();
        old_last_frame_.clear();
    }
    for (auto* engine : {pending.get(), current.get()}) {
        if (!engine) continue;
        engine->clear_callbacks();
        engine->stop();
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

void SpotifyMVDesktop::fail_pending(const std::uint64_t generation, const std::string& track_id) {
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        if (!track_requests_.abandon(generation))
            return; // superseded: a newer request owns the status now
    }
    total_errors_++;
    send_websocket_message("track:error:" + track_id);
    send_websocket_message("status:error");
}

std::int64_t SpotifyMVDesktop::steady_now_ns()
{
    // 0 is reserved for "never", so a clock at its epoch cannot look unreported.
    return std::max<std::int64_t>(
        1, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
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

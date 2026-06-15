#include "SpotifyMVDesktop.h"
#include "SpotifyMVPacket.h"
#include "YouTubeSearcher.h"
#include "shared/desktop/utils.h"
#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

extern "C" PLUGIN_EXPORT SpotifyMVDesktop* createSpotifyMV() {
    return new SpotifyMVDesktop();
}

extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVDesktop* c) { delete c; }

SpotifyMVDesktop::~SpotifyMVDesktop() {
    if (search_thread_.joinable()) search_thread_.join();
    if (engine_) engine_->stop();
}

void SpotifyMVDesktop::post_init() {
    auto cacheRoot = get_data_dir() / "cache" / "spotifymv";
    std::filesystem::create_directories(cacheRoot);
    engine_ = std::make_unique<Shared::VideoStreamEngine>(cacheRoot, kWidth, kHeight);
    auto err = engine_->check_tools();
    tools_available_ = err.empty();
    if (!err.empty()) {
        tools_error_msg_ = err;
        spdlog::error(tools_error_msg_);
    }
    engine_->on_status_change = [this](const std::string& s) {
        send_websocket_message("status:" + s);
    };
    engine_->set_chunk_duration_sec(20);
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

    ImGui::Text("Track ID: %s", current_track_id_.empty() ? "None" : current_track_id_.c_str());
    ImGui::Text("URL: %s", engine_->get_current_url().empty() ? "None" : engine_->get_current_url().c_str());

    const char* stateStr = "Unknown";
    ImVec4 stateColor = {1, 1, 1, 1};
    switch (engine_->get_state()) {
    case Shared::VideoStreamEngine::State::Idle:        stateStr = "Idle";        break;
    case Shared::VideoStreamEngine::State::Downloading: stateStr = "Downloading"; stateColor = {1, 1, 0, 1}; break;
    case Shared::VideoStreamEngine::State::Playing:     stateStr = "Playing";     stateColor = {0, 1, 0, 1}; break;
    case Shared::VideoStreamEngine::State::Error:       stateStr = "Error";       stateColor = {1, 0, 0, 1}; break;
    }
    ImGui::TextColored(stateColor, "State: %s", stateStr);
    if (search_running_.load())
        ImGui::TextColored(ImVec4(0, 0.84f, 0.38f, 1), "YouTube search in progress...");

    if (engine_->get_state() == Shared::VideoStreamEngine::State::Error)
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s", engine_->get_last_error().c_str());
}

std::optional<std::unique_ptr<UdpPacket, void(*)(UdpPacket*)>>
SpotifyMVDesktop::compute_next_packet(const std::string sceneName) {
    if (sceneName != "spotifymv") return std::nullopt;
    if (!tools_available_) return std::nullopt;
    if (engine_->get_state() != Shared::VideoStreamEngine::State::Playing)
        return std::nullopt;
    if (!engine_->tick()) return std::nullopt;

    auto frame = engine_->get_current_frame();
    if (frame.empty()) return std::nullopt;

    return std::unique_ptr<UdpPacket, void(*)(UdpPacket*)>(
        new SpotifyMVPacket(std::move(frame)),
        [](UdpPacket* p) { delete static_cast<SpotifyMVPacket*>(p); });
}

void SpotifyMVDesktop::on_websocket_message(const std::string message) {
    if (message.starts_with("track:")) {
        std::string remainder = message.substr(6);

        auto colon_pos = remainder.find(':');
        if (colon_pos == std::string::npos) return;
        std::string track_id = remainder.substr(0, colon_pos);

        if (track_id == current_track_id_) return;

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

        long progress_ms = std::stol(progress_ms_str);
        long duration_ms = std::stol(duration_ms_str);

        engine_->stop();
        current_track_id_ = track_id;
        search_and_play(track_id, song, artist, suffix, fallback, progress_ms, duration_ms);
        return;
    }

    if (message == "stop") {
        engine_->stop();
        current_track_id_ = "";
        send_websocket_message("status:idle");
        return;
    }
}

void SpotifyMVDesktop::search_and_play(const std::string& track_id,
                                        const std::string& song,
                                        const std::string& artist,
                                        const std::string& suffix,
                                        bool fallback,
                                        long spotify_progress_ms,
                                        long spotify_duration_ms) {
    if (search_thread_.joinable()) search_thread_.join();

    search_running_ = true;
    send_websocket_message("status:searching");

    search_thread_ = std::thread([this, track_id, song, artist, suffix, fallback,
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
                return;
            }

            long seek_ms = spotify_progress_ms;
            spdlog::info("SpotifyMV: seeking to {}ms (track {}s long)",
                         seek_ms, spotify_duration_ms / 1000);
            engine_->start(url, track_id, seek_ms);
        } catch (const std::exception& e) {
            spdlog::error("SpotifyMV search exception: {}", e.what());
            send_websocket_message("status:error");
        }
        search_running_ = false;
    });
}

#include "shared/desktop/VideoStreamEngine.h"
#include "VideoStreamEngineInternal.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <cstdio>
#include <fmt/format.h>
#include <thread>
#include <spdlog/spdlog.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace Shared {

bool VideoStreamEngine::download_and_process_chunk(int chunk_index,
                                                    bool set_error_on_fail,
                                                    int start_sec_override) {
    try {
        const int normal_start = chunk_index * chunk_duration_sec_;
        const int start_sec = (start_sec_override >= 0) ? start_sec_override : normal_start;
        const int end_sec   = start_sec + chunk_duration_sec_;

        auto mp4Path = chunk_mp4_path(chunk_index);
        auto binPath = chunk_bin_path(chunk_index);

        std::error_code ec;
        // When using an override (seek-based offset), always re-download —
        // a different seek position means different content.
        if (start_sec_override < 0 &&
            std::filesystem::exists(binPath, ec) &&
            std::filesystem::file_size(binPath, ec) > 0) {
            spdlog::info("Chunk {} already on disk, skipping download", chunk_index);
            return true;
        }

        std::string dlCmd = build_ytdlp_command(mp4Path, start_sec, end_sec);
        spdlog::info("Downloading chunk {} ({}-{}s)", chunk_index, start_sec, end_sec);
        const auto dl = run_command_capture(dlCmd, &running_);
        if (dl.exit_code != 0) {
            if (dl.exit_code == -2 || !running_.load()) {
                spdlog::info("yt-dlp chunk {} interrupted by stop()", chunk_index);
                return false;
            }
            const auto detail = detail::command_error_summary(dl.output);
            std::string msg = fmt::format("yt-dlp chunk {} failed (exit {})", chunk_index, dl.exit_code);
            if (!detail.empty())
                msg += ": " + detail;
            if (set_error_on_fail) {
                set_last_error(msg);
                spdlog::error("{}", msg);
                state_ = State::Error;
                notify_status("error");
            } else {
                spdlog::info("{} — video likely ended", msg);
            }
            return false;
        }

        std::string ffCmd = build_ffmpeg_command(mp4Path, binPath);
        spdlog::info("Processing chunk {} to {}x{} @ {}fps", chunk_index,
                     width_, height_, fps_);
        const auto ff = run_command_capture(ffCmd, &running_);
        if (ff.exit_code != 0) {
            if (ff.exit_code == -2 || !running_.load()) {
                spdlog::info("ffmpeg chunk {} interrupted by stop()", chunk_index);
                return false;
            }
            const auto detail = detail::command_error_summary(ff.output);
            std::string msg = fmt::format("ffmpeg chunk {} failed (exit {})", chunk_index, ff.exit_code);
            if (!detail.empty())
                msg += ": " + detail;
            if (set_error_on_fail) {
                set_last_error(msg);
                spdlog::error("{}", msg);
                state_ = State::Error;
                notify_status("error");
            } else {
                spdlog::warn("{}", msg);
            }
            return false;
        }

        std::error_code ec2;
        if (std::filesystem::file_size(binPath, ec2) == 0) {
            std::filesystem::remove(binPath, ec2);
            std::string msg = fmt::format("ffmpeg chunk {} produced empty output — video ended", chunk_index);
            if (set_error_on_fail) {
                set_last_error(msg);
                spdlog::error("{}", msg);
                state_ = State::Error;
                notify_status("error");
            } else {
                spdlog::info("{}", msg);
            }
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception in download_and_process_chunk({}): {}", chunk_index, e.what());
        if (set_error_on_fail) {
            set_last_error(std::string("Chunk error: ") + e.what());
            state_ = State::Error;
            notify_status("error");
        }
        return false;
    }
}

std::filesystem::path VideoStreamEngine::chunk_mp4_path(int chunk_index) const {
    auto dir = cache_root_ / effective_cache_key();
    std::filesystem::create_directories(dir);
    return dir / fmt::format("chunk_{}.mp4", chunk_index);
}

std::filesystem::path VideoStreamEngine::chunk_bin_path(int chunk_index) const {
    auto dir = cache_root_ / effective_cache_key();
    std::filesystem::create_directories(dir);
    return dir / fmt::format("chunk_{}.bin", chunk_index);
}

std::filesystem::path VideoStreamEngine::fast_chunk_mp4_path() const {
    auto dir = cache_root_ / effective_cache_key();
    std::filesystem::create_directories(dir);
    return dir / "fast_chunk.mp4";
}

std::string VideoStreamEngine::effective_cache_key() const {
    if (!cache_key_.empty())
        return cache_key_;
    return std::to_string(std::hash<std::string>{}(current_url_));
}

void VideoStreamEngine::cleanup_chunk(int chunk_index) {
    if (chunk_index <= 0) return;
    std::error_code ec;
    std::filesystem::remove(chunk_mp4_path(chunk_index), ec);
    std::filesystem::remove(chunk_bin_path(chunk_index), ec);
}

void VideoStreamEngine::cleanup_non_first_chunks() {
    auto cacheDir = cache_root_ / effective_cache_key();
    if (!std::filesystem::exists(cacheDir)) return;

    auto keep_mp4 = chunk_mp4_path(0).filename();
    auto keep_bin = chunk_bin_path(0).filename();

    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(cacheDir, ec)) {
        auto name = entry.path().filename();
        if (name == keep_mp4 || name == keep_bin) continue;
        if (entry.path().extension() == ".mp4" || entry.path().extension() == ".bin")
            std::filesystem::remove(entry, ec);
    }
}

void VideoStreamEngine::evict_oldest_first_chunks() {
    std::error_code ec;
    if (!std::filesystem::exists(cache_root_, ec)) return;

    std::string current_key = effective_cache_key();

    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> others;
    for (auto& entry : std::filesystem::directory_iterator(cache_root_, ec)) {
        if (!entry.is_directory(ec)) continue;
        if (entry.path().filename() == current_key) continue;
        others.emplace_back(entry.last_write_time(ec), entry.path());
    }

    int allowed = MAX_FIRST_CHUNK_CACHE - 1;
    if (static_cast<int>(others.size()) <= allowed) return;

    std::sort(others.begin(), others.end());
    int excess = static_cast<int>(others.size()) - allowed;
    for (int i = 0; i < excess; ++i) {
        spdlog::info("Evicting old video cache: {}", others[i].second.string());
        std::filesystem::remove_all(others[i].second, ec);
    }
}

} // namespace Shared

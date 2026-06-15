#include "shared/desktop/VideoStreamEngine.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

inline const char* null_device() {
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

inline void close_pipe(FILE* pipe) {
    if (!pipe) return;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
}

inline int run_command(const std::string& cmd) {
#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::string fullCmd = "cmd.exe /C " + cmd;
    std::vector<char> cmdline(fullCmd.begin(), fullCmd.end());
    cmdline.push_back('\0');
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    return system(cmd.c_str());
#endif
}

} // anonymous namespace

namespace Shared {

VideoStreamEngine::VideoStreamEngine(std::filesystem::path cache_root,
                                     int width, int height, double fps)
    : cache_root_(std::move(cache_root)),
      width_(width), height_(height), fps_(fps) {
    last_frame_time_ = std::chrono::steady_clock::now();
}

VideoStreamEngine::~VideoStreamEngine() {
    stop();
}

std::string VideoStreamEngine::check_tools() {
    try {
        if (run_command(fmt::format("ffmpeg -version > {} 2>&1", null_device())) != 0) {
            return "ffmpeg not found in PATH.";
        }
        if (run_command(fmt::format("yt-dlp --version > {} 2>&1", null_device())) != 0) {
            return "yt-dlp not found in PATH.";
        }
        spdlog::info("ffmpeg and yt-dlp found.");
        return "";
    } catch (const std::exception& e) {
        return std::string("Error checking tools: ") + e.what();
    }
}

void VideoStreamEngine::start(const std::string& url, const std::string& cache_key) {
    if (state_.load() == State::Playing && url == current_url_ && cache_key == cache_key_) {
        return; // Already streaming this URL
    }

    stop(); // Stop any existing stream

    current_url_ = url;
    cache_key_ = cache_key;
    running_ = true;
    last_frame_time_ = std::chrono::steady_clock::now();

    processing_thread_ = std::thread([this]() {
        try {
            const size_t frameSize = static_cast<size_t>(width_) *
                                     static_cast<size_t>(height_) * 3;

            auto play_chunk = [&](int chunk_index) -> bool {
                try {
                    spdlog::info("Playing chunk {}", chunk_index);
                    std::string binPath = chunk_bin_path(chunk_index).string();
                    FILE* bin = fopen(binPath.c_str(), "rb");
                    if (!bin) {
                        last_error_ = "Failed to open chunk: " + binPath;
                        spdlog::error(last_error_);
                        state_ = State::Error;
                        notify_status("error");
                        return false;
                    }

                    state_ = State::Playing;
                    notify_status("playing");

                    std::vector<uint8_t> buffer(frameSize);
                    while (running_) {
                        if (fread(buffer.data(), 1, frameSize, bin) != frameSize)
                            break;
                        {
                            std::lock_guard<std::mutex> lock(frame_mutex_);
                            current_frame_ = buffer;
                        }
                        std::this_thread::sleep_for(
                            std::chrono::duration<double>(1.0 / fps_));
                    }

                    fclose(bin);
                    return running_.load();
                } catch (const std::exception& e) {
                    spdlog::error("Exception playing chunk {}: {}", chunk_index, e.what());
                    last_error_ = std::string("Playback error: ") + e.what();
                    state_ = State::Error;
                    notify_status("error");
                    return false;
                }
            };

            while (running_) {
                std::string bin0 = chunk_bin_path(0).string();
                std::error_code ec;
                bool cached = std::filesystem::exists(bin0, ec) &&
                              std::filesystem::file_size(bin0, ec) > 0;

                if (!cached) {
                    state_ = State::Downloading;
                    notify_status("downloading");
                    spdlog::info("Downloading initial chunk 0");
                    if (!download_and_process_chunk(0)) {
                        break;
                    }
                    evict_oldest_first_chunks();
                } else {
                    spdlog::info("Using cached chunk 0 — starting immediately");
                }

                int current = 0;
                bool had_error = false;

                while (running_) {
                    int next = current + 1;

                    std::atomic<bool> prefetch_success{false};
                    prefetch_thread_ = std::thread([this, next, &prefetch_success]() {
                        try {
                            prefetch_success = download_and_process_chunk(next, false);
                            spdlog::info("Prefetch chunk {}: {}",
                                         next,
                                         prefetch_success.load() ? "ok" : "failed (end of video)");
                        } catch (const std::exception& e) {
                            spdlog::warn("Prefetch chunk {} exception: {}", next, e.what());
                        }
                    });

                    if (!play_chunk(current)) {
                        if (prefetch_thread_.joinable()) prefetch_thread_.join();
                        had_error = (state_.load() == State::Error);
                        break;
                    }

                    if (prefetch_thread_.joinable()) {
                        spdlog::debug("Waiting for prefetch of chunk {}", next);
                        prefetch_thread_.join();
                    }

                    if (current > 0)
                        cleanup_chunk(current);

                    if (!prefetch_success) {
                        spdlog::info("Video ended — looping from chunk 0");
                        cleanup_non_first_chunks();
                        break;
                    }

                    current = next;
                }

                if (had_error) break;
            }

            running_ = false;
            if (state_.load() != State::Error) {
                state_ = State::Idle;
                notify_status("idle");
            }
        } catch (const std::exception& e) {
            spdlog::error("Critical exception in streaming thread: {}", e.what());
            last_error_ = std::string("Streaming error: ") + e.what();
            state_ = State::Error;
            notify_status("error");
            running_ = false;
        } catch (...) {
            spdlog::error("Unknown exception in streaming thread");
            last_error_ = "Unknown streaming error";
            state_ = State::Error;
            notify_status("error");
            running_ = false;
        }
    });
}

void VideoStreamEngine::stop() {
    running_ = false;

    if (processing_thread_.joinable()) {
        try {
            processing_thread_.join();
        } catch (const std::exception& e) {
            spdlog::error("Exception joining processing thread: {}", e.what());
        }
    }
    if (prefetch_thread_.joinable())
        prefetch_thread_.join();

    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_.clear();
    }
    state_ = State::Idle;
}

std::vector<uint8_t> VideoStreamEngine::get_current_frame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return current_frame_;
}

bool VideoStreamEngine::tick() {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - last_frame_time_).count() < 1.0 / fps_)
        return false;
    last_frame_time_ = now;
    return true;
}

bool VideoStreamEngine::download_and_process_chunk(int chunk_index,
                                                    bool set_error_on_fail) {
    try {
        const int start_sec = chunk_index * CHUNK_DURATION_SEC;
        const int end_sec   = start_sec + CHUNK_DURATION_SEC;

        auto mp4Path = chunk_mp4_path(chunk_index);
        auto binPath = chunk_bin_path(chunk_index);

        std::error_code ec;
        if (std::filesystem::exists(binPath, ec) &&
            std::filesystem::file_size(binPath, ec) > 0) {
            spdlog::info("Chunk {} already on disk, skipping download", chunk_index);
            return true;
        }

        std::string dlCmd = fmt::format(
            "yt-dlp -f \"best[ext=mp4]/best\" --download-sections \"*{}-{}\" "
            "--force-overwrites -o \"{}\" \"{}\"",
            start_sec, end_sec, mp4Path.string(), current_url_);
        spdlog::info("Downloading chunk {} ({}-{}s)", chunk_index, start_sec, end_sec);
        int dlResult = run_command(dlCmd);
        if (dlResult != 0) {
            std::string msg = fmt::format("yt-dlp chunk {} failed (exit {})", chunk_index, dlResult);
            if (set_error_on_fail) {
                last_error_ = msg;
                spdlog::error(last_error_);
                state_ = State::Error;
                notify_status("error");
            } else {
                spdlog::info("{} — video likely ended", msg);
            }
            return false;
        }

        std::string ffCmd = fmt::format(
            "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
            "-f rawvideo -pix_fmt rgb24 \"{}\"",
            mp4Path.string(), width_, height_, fps_, binPath.string());
        spdlog::info("Processing chunk {} to {}x{} @ {}fps", chunk_index,
                     width_, height_, fps_);
        int ffResult = run_command(ffCmd);
        if (ffResult != 0) {
            std::string msg = fmt::format("ffmpeg chunk {} failed (exit {})", chunk_index, ffResult);
            if (set_error_on_fail) {
                last_error_ = msg;
                spdlog::error(last_error_);
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
                last_error_ = msg;
                spdlog::error(last_error_);
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
            last_error_ = std::string("Chunk error: ") + e.what();
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

void VideoStreamEngine::notify_status(const std::string& s) {
    if (on_status_change)
        on_status_change(s);
}

} // namespace Shared

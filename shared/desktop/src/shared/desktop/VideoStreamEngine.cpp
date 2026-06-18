#include "shared/desktop/VideoStreamEngine.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <cstdio>
#include <fmt/format.h>
#include <thread>
#include <spdlog/spdlog.h>

namespace {

inline const char* null_device() {
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
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

void VideoStreamEngine::start(const std::string& url, const std::string& cache_key, long seek_ms) {
    if (state_.load() == State::Playing && url == current_url_ && cache_key == cache_key_) {
        return;
    }

    stop();

    current_url_ = url;
    cache_key_ = cache_key;
    seek_ms_.store(seek_ms);
    running_ = true;
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        last_frame_time_ = std::chrono::steady_clock::now();
    }

    processing_thread_ = std::thread([this]() {
        try {
            const size_t frameSize = static_cast<size_t>(width_) *
                                     static_cast<size_t>(height_) * 3;

            auto play_chunk = [&](int chunk_index, int skip_frames = 0) -> bool {
                try {
                    spdlog::info("Playing chunk {} (skip {} frames)", chunk_index, skip_frames);
                    std::string binPath = chunk_bin_path(chunk_index).string();
                    FILE* bin = fopen(binPath.c_str(), "rb");
                    if (!bin) {
                        set_last_error("Failed to open chunk: " + binPath);
                        spdlog::error(last_error_);
                        state_ = State::Error;
                        notify_status("error");
                        return false;
                    }

                    state_ = State::Playing;
                    notify_status("playing");

                    std::vector<uint8_t> buffer(frameSize);

                    for (int i = 0; i < skip_frames && running_; ++i) {
                        if (fread(buffer.data(), 1, frameSize, bin) != frameSize) break;
                    }

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
                    set_last_error(std::string("Playback error: ") + e.what());
                    state_ = State::Error;
                    notify_status("error");
                    return false;
                }
            };

            while (running_) {
                long seek = seek_ms_.exchange(0);
                int start_chunk = 0;
                int skip_frames = 0;
                if (seek > 0) {
                    start_chunk = static_cast<int>(seek / (chunk_duration_sec_ * 1000));
                    skip_frames = static_cast<int>(
                        (seek % (chunk_duration_sec_ * 1000)) * fps_ / 1000.0);
                    spdlog::info("Seek: {}ms \u2192 chunk {} skip {} frames",
                                 seek, start_chunk, skip_frames);
                }

                std::string startBin = chunk_bin_path(start_chunk).string();
                std::error_code ec;
                bool cached = std::filesystem::exists(startBin, ec) &&
                              std::filesystem::file_size(startBin, ec) > 0;

                if (!cached) {
                    state_ = State::Downloading;
                    notify_status("downloading");
                    spdlog::info("Downloading initial chunk {}", start_chunk);

                    // Fast-start: kick off background download of the full chunk 0,
                    // then immediately stream a short clip so playback starts in seconds.
                    int fast_start_sec = start_chunk * chunk_duration_sec_ +
                                         static_cast<int>(skip_frames / fps_);
                    auto full_chunk_done = std::make_shared<std::atomic<bool>>(false);
                    auto full_chunk_ok   = std::make_shared<std::atomic<bool>>(false);
                    std::thread full_chunk_thread([this, start_chunk, full_chunk_done, full_chunk_ok]() {
                        *full_chunk_ok = download_and_process_chunk(start_chunk);
                        if (start_chunk == 0 && full_chunk_ok->load())
                            evict_oldest_first_chunks();
                        *full_chunk_done = true;
                    });

                    // Play the fast clip while the full chunk downloads in the background.
                    // If fast start fails (e.g. yt-dlp flake) we fall through and wait
                    // for the full chunk like before — no UX regression.
                    if (running_.load()) {
                        play_fast_chunk(fast_start_sec, fast_chunk_duration_sec_);
                    }

                    // Now wait for the full chunk to be ready
                    if (full_chunk_thread.joinable()) {
                        // Show downloading state again while we wait (fast clip has ended)
                        if (running_.load() && !full_chunk_done->load()) {
                            state_ = State::Downloading;
                            notify_status("downloading");
                        }
                        full_chunk_thread.join();
                    }

                    if (!full_chunk_ok->load()) {
                        break;
                    }
                    // Advance skip_frames past what the fast chunk already displayed,
                    // so the normal play_chunk doesn't replay those frames.
                    skip_frames = static_cast<int>(fast_start_sec * fps_) +
                                  static_cast<int>(fast_chunk_duration_sec_ * fps_);
                    // Clamp to chunk size so we don't skip past the end
                    int max_skip = static_cast<int>(chunk_duration_sec_ * fps_);
                    if (skip_frames > max_skip) skip_frames = max_skip;
                } else {
                    spdlog::info("Using cached chunk {} — starting immediately", start_chunk);
                }

                int current = start_chunk;
                bool had_error = false;
                bool first_chunk = true;

                while (running_) {
                    int next = current + 1;

                    // Use shared_ptr so the atomic outlives any potential lambda/thread lifetime mismatch
                    auto prefetch_success = std::make_shared<std::atomic<bool>>(false);
                    {
                        std::lock_guard<std::mutex> lk(prefetch_mutex_);
                        if (prefetch_thread_.joinable()) prefetch_thread_.join(); // safety guard
                        prefetch_thread_ = std::thread([this, next, prefetch_success]() {
                        try {
                            *prefetch_success = download_and_process_chunk(next, false);
                            spdlog::info("Prefetch chunk {}: {}",
                                         next,
                                         prefetch_success->load() ? "ok" : "failed (end of video)");
                        } catch (const std::exception& e) {
                            spdlog::warn("Prefetch chunk {} exception: {}", next, e.what());
                        }
                    });
                    } // prefetch_mutex_ released here — stop() can now join if needed

                    int this_skip = first_chunk ? skip_frames : 0;
                    if (!play_chunk(current, this_skip)) {
                        {
                            std::lock_guard<std::mutex> lk(prefetch_mutex_);
                            if (prefetch_thread_.joinable()) prefetch_thread_.join();
                        }
                        had_error = (state_.load() == State::Error);
                        break;
                    }
                    first_chunk = false;

                    {
                        std::lock_guard<std::mutex> lk(prefetch_mutex_);
                        if (prefetch_thread_.joinable()) {
                            spdlog::debug("Waiting for prefetch of chunk {}", next);
                            prefetch_thread_.join();
                        }
                    }

                    if (current > 0)
                        cleanup_chunk(current);

                    if (!prefetch_success->load()) {
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
            set_last_error(std::string("Streaming error: ") + e.what());
            state_ = State::Error;
            notify_status("error");
            running_ = false;
        } catch (...) {
            spdlog::error("Unknown exception in streaming thread");
            set_last_error("Unknown streaming error");
            state_ = State::Error;
            notify_status("error");
            running_ = false;
        }
    });
}

// Downloads a short clip with yt-dlp and pipes ffmpeg output directly to the
// frame buffer so playback starts on the first decoded frame, without waiting
// for a complete .bin file to be written to disk first.
bool VideoStreamEngine::play_fast_chunk(int start_sec, int duration_sec) {
    if (!running_.load()) return false;

    auto mp4Path = fast_chunk_mp4_path();
    std::error_code ec;
    std::filesystem::remove(mp4Path, ec); // always re-download; it's a temp file

    // Download the short clip
    std::string dlCmd = build_ytdlp_command(mp4Path, start_sec, start_sec + duration_sec);
    spdlog::info("Fast chunk: downloading {}s clip starting at {}s", duration_sec, start_sec);
    int dlResult = run_command(dlCmd, &running_);
    if (dlResult != 0) {
        spdlog::warn("Fast chunk download failed (exit {}), skipping fast start", dlResult);
        return false;
    }
    if (!running_.load()) return false;

    // Pipe ffmpeg raw output directly — frames arrive as ffmpeg encodes them
    const size_t frameSize = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
    std::string ffCmd = build_ffmpeg_pipe_command(mp4Path);
#ifdef _WIN32
    FILE* pipe = _popen(ffCmd.c_str(), "rb");
#else
    FILE* pipe = popen(ffCmd.c_str(), "r");
#endif
    if (!pipe) {
        spdlog::warn("Fast chunk: failed to open ffmpeg pipe, skipping fast start");
        return false;
    }

    state_ = State::Playing;
    notify_status("playing");

    std::vector<uint8_t> buffer(frameSize);
    int frames_played = 0;
    while (running_) {
        size_t got = fread(buffer.data(), 1, frameSize, pipe);
        if (got != frameSize) break; // ffmpeg finished or error
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            current_frame_ = buffer;
        }
        ++frames_played;
        std::this_thread::sleep_for(std::chrono::duration<double>(1.0 / fps_));
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    // Clean up the temp mp4 — chunk 0 .mp4 will be downloaded separately
    std::filesystem::remove(mp4Path, ec);
    spdlog::info("Fast chunk: played {} frames, handing off to normal playback", frames_played);
    return running_.load();
}

void VideoStreamEngine::stop() {
    running_ = false;

    std::function<void(const std::string &)> tmp_status_change;
    // Clear the status callback BEFORE joining threads.
    // The processing_thread_ calls notify_status() just before it exits, which fires
    // on_status_change — a lambda that typically captures the parent plugin's `this`
    // pointer (e.g. to call send_websocket_message). If stop() is called from the
    // parent's destructor, that `this` may already be partially destroyed by the time
    // the thread fires the callback. Clearing it here makes the callback a no-op,
    // preventing a use-after-free crash.
    {
        std::lock_guard<std::mutex> lk(status_cb_mutex_);
        tmp_status_change = on_status_change;
        on_status_change = nullptr;
    }

    // Join prefetch_thread_ first: processing_thread_ may be blocked waiting for it,
    // so joining processing_thread_ first would deadlock.
    {
        std::lock_guard<std::mutex> lk(prefetch_mutex_);
        if (prefetch_thread_.joinable())
            prefetch_thread_.join();
    }

    if (processing_thread_.joinable()) {
        try {
            processing_thread_.join();
        } catch (const std::exception& e) {
            spdlog::error("Exception joining processing thread: {}", e.what());
        }
    }

    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_.clear();
    }
    on_status_change = tmp_status_change;
    state_ = State::Idle;
}

// ---- Command construction helpers ----
std::string VideoStreamEngine::build_ytdlp_command(
    const std::filesystem::path& output_path, int start_sec, int end_sec) const
{
    return fmt::format(
        "yt-dlp -f \"best[ext=mp4]/best\" --download-sections \"*{}-{}\" "
        "--force-overwrites -o \"{}\" \"{}\"",
        start_sec, end_sec, output_path.string(), current_url_);
}

std::string VideoStreamEngine::build_ffmpeg_command(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path) const
{
    return fmt::format(
        "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
        "-f rawvideo -pix_fmt rgb24 \"{}\"",
        input_path.string(), width_, height_, fps_, output_path.string());
}

std::string VideoStreamEngine::build_ffmpeg_pipe_command(
    const std::filesystem::path& input_path) const
{
#ifdef _WIN32
    return fmt::format(
        "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
        "-f rawvideo -pix_fmt rgb24 pipe:1 2>NUL",
        input_path.string(), width_, height_, fps_);
#else
    return fmt::format(
        "ffmpeg -y -i '{}' -vf 'scale={}:{},setsar=1:1,fps={}' "
        "-f rawvideo -pix_fmt rgb24 pipe:1 2>/dev/null",
        input_path.string(), width_, height_, fps_);
#endif
}

std::vector<uint8_t> VideoStreamEngine::get_current_frame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return current_frame_;
}

bool VideoStreamEngine::tick() {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (std::chrono::duration<double>(now - last_frame_time_).count() < 1.0 / fps_)
        return false;
    last_frame_time_ = now;
    return true;
}

bool VideoStreamEngine::download_and_process_chunk(int chunk_index,
                                                    bool set_error_on_fail) {
    try {
        const int start_sec = chunk_index * chunk_duration_sec_;
        const int end_sec   = start_sec + chunk_duration_sec_;

        auto mp4Path = chunk_mp4_path(chunk_index);
        auto binPath = chunk_bin_path(chunk_index);

        std::error_code ec;
        if (std::filesystem::exists(binPath, ec) &&
            std::filesystem::file_size(binPath, ec) > 0) {
            spdlog::info("Chunk {} already on disk, skipping download", chunk_index);
            return true;
        }

        std::string dlCmd = build_ytdlp_command(mp4Path, start_sec, end_sec);
        spdlog::info("Downloading chunk {} ({}-{}s)", chunk_index, start_sec, end_sec);
        int dlResult = run_command(dlCmd, &running_);
        if (dlResult != 0) {
            if (dlResult == -2 || !running_.load()) {
                spdlog::info("yt-dlp chunk {} interrupted by stop()", chunk_index);
                return false;
            }
            std::string msg = fmt::format("yt-dlp chunk {} failed (exit {})", chunk_index, dlResult);
            if (set_error_on_fail) {
                set_last_error(msg);
                spdlog::error(last_error_);
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
        int ffResult = run_command(ffCmd, &running_);
        if (ffResult != 0) {
            if (ffResult == -2 || !running_.load()) {
                spdlog::info("ffmpeg chunk {} interrupted by stop()", chunk_index);
                return false;
            }
            std::string msg = fmt::format("ffmpeg chunk {} failed (exit {})", chunk_index, ffResult);
            if (set_error_on_fail) {
                set_last_error(msg);
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
                set_last_error(msg);
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

void VideoStreamEngine::set_last_error(const std::string& msg) {
    std::lock_guard<std::mutex> lk(error_mutex_);
    last_error_ = msg;
}

void VideoStreamEngine::notify_status(const std::string& s) {
    std::lock_guard<std::mutex> lk(status_cb_mutex_);
    if (on_status_change)
        on_status_change(s);
}

} // namespace Shared

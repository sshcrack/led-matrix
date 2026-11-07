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
    auto error = check_video_tools_available();
    if (error.empty())
        spdlog::info("ffmpeg and yt-dlp found.");
    return error;
}

void VideoStreamEngine::start(const std::string& url, const std::string& cache_key, long seek_ms) {
    if (state_.load() == State::Playing && url == current_url_ && cache_key == cache_key_) {
        return;
    }

    stop();
    first_frame_fired_ = false;

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
            if (continuous_streaming_) {
                play_continuous_stream(seek_ms_.exchange(0));
                running_ = false;
                return;
            }
            const size_t frameSize = static_cast<size_t>(width_) *
                                     static_cast<size_t>(height_) * 3;

            auto play_chunk = [&](int chunk_index, int skip_frames = 0) -> bool {
                try {
                    spdlog::info("Playing chunk {} (skip {} frames)", chunk_index, skip_frames);
                    std::string binPath = chunk_bin_path(chunk_index).string();
                    FILE* bin = fopen(binPath.c_str(), "rb");
                    if (!bin) {
                        set_last_error("Failed to open chunk: " + binPath);
                        spdlog::error("Failed to open chunk: {}", binPath);
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
                        std::function<void()> first_frame_cb;
                        {
                            std::lock_guard<std::mutex> lk(status_cb_mutex_);
                            if (on_first_frame_ready && !first_frame_fired_.exchange(true))
                                first_frame_cb = on_first_frame_ready;
                        }
                        if (first_frame_cb)
                            first_frame_cb();
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
                    // Align the initial full-chunk download with the seek point
                    // (fast_start_sec) instead of the fixed chunk boundary.
                    // This avoids wasting bandwidth on data before the seek and
                    // gives the full chunk_duration_sec_ of play-time before the
                    // prefetch of the next chunk is needed, eliminating the
                    // "chunk finished before prefetch" stall when starting mid-chunk.
                    int initial_chunk_sec = fast_start_sec;
                    auto full_chunk_done = std::make_shared<std::atomic<bool>>(false);
                    auto full_chunk_ok   = std::make_shared<std::atomic<bool>>(false);
                    std::thread full_chunk_thread([this, start_chunk, initial_chunk_sec,
                                                    full_chunk_done, full_chunk_ok]() {
                        *full_chunk_ok = download_and_process_chunk(
                            start_chunk, true, initial_chunk_sec);
                        if (start_chunk == 0 && full_chunk_ok->load())
                            evict_oldest_first_chunks();
                        *full_chunk_done = true;
                    });

                    // Play the fast clip while the full chunk downloads in the background.
                    // If stop() races the fast clip, remember the cancellation but
                    // still join full_chunk_thread before leaving this scope. Destroying
                    // a joinable std::thread calls std::terminate(), which used to make
                    // quick track changes / shutdowns crash the desktop here.
                    bool fast_start_cancelled = false;
                    if (running_.load()) {
                        if (!play_fast_chunk(fast_start_sec, fast_chunk_duration_sec_) && !running_.load())
                            fast_start_cancelled = true;
                    }

                    // Now wait for the full chunk to be ready. This join is mandatory
                    // even on cancellation; full_chunk_thread owns a cancellable yt-dlp
                    // child and observes running_ becoming false.
                    if (full_chunk_thread.joinable()) {
                        // Show downloading state again while we wait (fast clip has ended)
                        if (running_.load() && !full_chunk_done->load()) {
                            state_ = State::Downloading;
                            notify_status("downloading");
                        }
                        full_chunk_thread.join();
                    }

                    if (fast_start_cancelled || !full_chunk_ok->load())
                        break;
                    // The full chunk now starts at fast_start_sec (aligned to the
                    // seek point), so we only skip frames the fast chunk already
                    // played — not the gap between the chunk boundary and the seek.
                    skip_frames = static_cast<int>(fast_chunk_duration_sec_ * fps_);
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

                    // Use shared_ptr so the atomics outlive any potential lambda/thread lifetime mismatch
                    auto prefetch_success = std::make_shared<std::atomic<bool>>(false);
                    auto prefetch_done = std::make_shared<std::atomic<bool>>(false);
                    {
                        std::lock_guard<std::mutex> lk(prefetch_mutex_);
                        if (prefetch_thread_.joinable()) prefetch_thread_.join(); // safety guard
                        prefetch_thread_ = std::thread([this, next, prefetch_success, prefetch_done]() {
                        try {
                            *prefetch_success = download_and_process_chunk(next, false);
                            spdlog::info("Prefetch chunk {}: {}",
                                         next,
                                         prefetch_success->load() ? "ok" : "failed (end of video)");
                        } catch (const std::exception& e) {
                            spdlog::warn("Prefetch chunk {} exception: {}", next, e.what());
                        }
                        *prefetch_done = true;
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
                            if (!prefetch_done->load()) {
                                // The chunk we just finished playing ran out before the
                                // next one was ready — without this, playback just
                                // freezes on the last frame with no indication why.
                                // Surface it as "downloading" so callers (e.g. the
                                // matrix-side loading overlay) can show buffering
                                // instead of looking stuck.
                                spdlog::info("Chunk {} finished before prefetch of {} was ready — buffering",
                                             current, next);
                                state_ = State::Downloading;
                                notify_status("downloading");
                            }
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

void VideoStreamEngine::stop() {
    running_ = false;

    // Kill ffmpeg child process to unblock fread() in play_decoded_pipe
    if (const pid_t pid = ffmpeg_pid_.load(); pid > 0) {
#ifdef _WIN32
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); }
#else
        // Only signal here: play_decoded_pipe() on the processing thread owns the
        // child and reaps it once fread() unblocks. Reaping it here as well
        // made the loser of the race spin on ECHILD for 2 s and signal a pid
        // that may already have been reused. While ffmpeg_pid_ still holds the
        // pid the child is unreaped, so the pid cannot have been recycled.
        kill(-pid, SIGTERM);
        for (int i = 0; i < 20 && ffmpeg_pid_.load() == pid; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (ffmpeg_pid_.load() == pid)
            kill(-pid, SIGKILL);
#endif
    }

    std::function<void(const std::string &)> tmp_status_change;
    std::function<void()> tmp_first_frame_ready;
    // Clear callbacks BEFORE joining threads.
    // The processing_thread_ calls notify_status() just before it exits, which fires
    // on_status_change — a lambda that typically captures the parent plugin's `this`
    // pointer (e.g. to call send_websocket_message). If stop() is called from the
    // parent's destructor, that `this` may already be partially destroyed by the time
    // the thread fires the callback. Clearing it here makes the callback a no-op,
    // preventing a use-after-free crash.
    {
        std::lock_guard<std::mutex> lk(status_cb_mutex_);
        tmp_status_change = on_status_change;
        tmp_first_frame_ready = on_first_frame_ready;
        on_status_change = nullptr;
        on_first_frame_ready = nullptr;
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
    {
        std::lock_guard<std::mutex> lk(status_cb_mutex_);
        on_status_change = std::move(tmp_status_change);
        on_first_frame_ready = std::move(tmp_first_frame_ready);
    }
    state_ = State::Idle;
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

void VideoStreamEngine::set_last_error(const std::string& msg) {
    std::lock_guard<std::mutex> lk(error_mutex_);
    last_error_ = msg;
}

void VideoStreamEngine::notify_status(const std::string& s) {
    std::function<void(const std::string&)> callback;
    {
        std::lock_guard<std::mutex> lk(status_cb_mutex_);
        callback = on_status_change;
    }
    if (callback)
        callback(s);
}

} // namespace Shared

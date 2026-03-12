#include "VideoDesktop.h"
#include "VideoPacket.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo() {
  return new VideoDesktop();
}

extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c) { delete c; }

namespace {
inline const char *null_device() {
#ifdef _WIN32
  return "NUL";
#else
  return "/dev/null";
#endif
}

inline void close_pipe(FILE *pipe) {
  if (!pipe) return;
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
}

inline int run_command(const std::string &cmd) {
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
} // namespace

VideoDesktop::~VideoDesktop() {
  stop_stream();
}

void VideoDesktop::check_tools() {
  try {
    if (run_command(fmt::format("ffmpeg -version > {} 2>&1", null_device())) != 0) {
      tools_available = false;
      tools_error_msg = "ffmpeg not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }
    if (run_command(fmt::format("yt-dlp --version > {} 2>&1", null_device())) != 0) {
      tools_available = false;
      tools_error_msg = "yt-dlp not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }
    if (run_command(fmt::format("ffplay -version > {} 2>&1", null_device())) != 0) {
      tools_available = false;
      tools_error_msg = "ffplay not found in PATH (needed for audio).";
      spdlog::error(tools_error_msg);
      return;
    }
    tools_available = true;
    spdlog::info("ffmpeg, ffplay, and yt-dlp found.");
  } catch (const std::exception &e) {
    tools_available = false;
    tools_error_msg = "Error checking tools: " + std::string(e.what());
    spdlog::error(tools_error_msg);
  }
}

void VideoDesktop::post_init() {
  check_tools();
  auto cacheDir = get_data_dir() / "cache" / "video";
  if (!std::filesystem::exists(cacheDir))
    std::filesystem::create_directories(cacheDir);
}

void VideoDesktop::load_config(std::optional<const nlohmann::json> config) {
  if (config.has_value()) {
    const auto &cfg = config.value();
    if (cfg.contains("enable_audio"))
      enable_audio = cfg["enable_audio"].get<bool>();
  }
}

void VideoDesktop::save_config(nlohmann::json &config) const {
  config["enable_audio"] = enable_audio;
}

void VideoDesktop::initialize_imgui(ImGuiContext *im_gui_context,
                                    ImGuiMemAllocFunc *alloc_fn,
                                    ImGuiMemFreeFunc *free_fn,
                                    void **user_data) {
  ImGui::SetCurrentContext(im_gui_context);
  ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void VideoDesktop::render() {
  if (!tools_available) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", tools_error_msg.c_str());
    return;
  }
  render_status_ui();
}

void VideoDesktop::render_status_ui() {
  ImGui::Text("URL: %s", current_url.empty() ? "None" : current_url.c_str());

  if (ImGui::Checkbox("Enable Audio", &enable_audio)) {
    if (!enable_audio) stop_audio();
  }

  const char *stateStr = "Unknown";
  ImVec4 stateColor = {1, 1, 1, 1};
  switch (state.load()) {
  case State::Idle:        stateStr = "Idle";        break;
  case State::Downloading: stateStr = "Downloading"; stateColor = {1, 1, 0, 1}; break;
  case State::Playing:     stateStr = "Playing";     stateColor = {0, 1, 0, 1}; break;
  case State::Error:       stateStr = "Error";       stateColor = {1, 0, 0, 1}; break;
  }
  ImGui::TextColored(stateColor, "Status: %s", stateStr);
  if (state.load() == State::Error)
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s", last_error.c_str());
}

void VideoDesktop::pre_new_frame() {}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
VideoDesktop::compute_next_packet(const std::string sceneName) {
  if (sceneName != "video" || state.load() != State::Playing ||
      !tools_available || !allow_sending_packets.load())
    return std::nullopt;

  const auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration<double>(now - last_packet_time).count() < 1.0 / fps)
    return std::nullopt;
  last_packet_time = now;

  std::lock_guard<std::mutex> lock(data_mutex);
  if (current_frame_data.empty()) return std::nullopt;

  return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(
      new VideoPacket(current_frame_data),
      [](UdpPacket *p) { delete dynamic_cast<VideoPacket *>(p); });
}

void VideoDesktop::on_websocket_message(const std::string message) {
  if (message == "stream:stop") {
    allow_sending_packets = false;
    stop_stream();
    return;
  }
  if (message == "stream:start") {
    allow_sending_packets = true;
    return;
  }
  if (message.starts_with("url:")) {
    if (!tools_available) {
      send_websocket_message("status:error");
      return;
    }
    std::string newUrl = message.substr(4);
    if (state.load() != State::Idle)
      stop_stream();
    current_url = newUrl;
    start_stream(newUrl);
    return;
  }
  if (message.starts_with("size:")) {
    std::string sizeStr = message.substr(5);
    auto xPos = sizeStr.find('x');
    matrix_width  = std::stoi(sizeStr.substr(0, xPos));
    matrix_height = std::stoi(sizeStr.substr(xPos + 1));
  }
}

// ─── Stream ──────────────────────────────────────────────────────────────────

void VideoDesktop::start_stream(const std::string &url) {
  streaming_thread_running = true;
  last_packet_time = std::chrono::steady_clock::now();

  processing_thread = std::thread([this, url]() {
    try {
      const size_t frameSize = static_cast<size_t>(matrix_width) *
                               static_cast<size_t>(matrix_height) * 3;

      // Plays the raw .bin file for a chunk frame-by-frame.
      // Returns true if the chunk completed or was cleanly stopped,
      // false on a hard error (state is set to Error).
      auto play_chunk = [&](int chunk_index) -> bool {
        try {
          spdlog::info("Playing chunk {}", chunk_index);
          std::string binPath = chunk_bin_path(chunk_index);
          FILE *bin = fopen(binPath.c_str(), "rb");
          if (!bin) {
            last_error = "Failed to open chunk: " + binPath;
            spdlog::error(last_error);
            state = State::Error;
            send_websocket_message("status:error");
            return false;
          }

          if (enable_audio)
            start_audio(chunk_mp4_path(chunk_index));

          state = State::Playing;
          send_websocket_message("status:playing");

          std::vector<uint8_t> buffer(frameSize);
          while (streaming_thread_running) {
            if (fread(buffer.data(), 1, frameSize, bin) != frameSize)
              break; // EOF or short read — chunk done
            {
              std::lock_guard<std::mutex> lock(data_mutex);
              current_frame_data = buffer;
            }
            std::this_thread::sleep_for(
                std::chrono::duration<double>(1.0 / fps));
          }

          fclose(bin);
          stop_audio();
          return streaming_thread_running.load();
        } catch (const std::exception &e) {
          spdlog::error("Exception playing chunk {}: {}", chunk_index, e.what());
          last_error = std::string("Playback error: ") + e.what();
          state = State::Error;
          send_websocket_message("status:error");
          return false;
        }
      };

      // Outer loop — restart from chunk 0 when the video ends.
      while (streaming_thread_running) {
        // ── Chunk 0: use cached copy if present, otherwise download ──
        std::string bin0 = chunk_bin_path(0);
        std::error_code ec;
        bool cached = std::filesystem::exists(bin0, ec) &&
                      std::filesystem::file_size(bin0, ec) > 0;

        if (!cached) {
          state = State::Downloading;
          send_websocket_message("status:downloading");
          spdlog::info("Downloading initial chunk 0");
          if (!download_and_process_chunk(url, 0)) {
            break; // Fatal — give up
          }
          evict_oldest_first_chunks(); // Keep cache within limit
        } else {
          spdlog::info("Using cached chunk 0 — starting immediately");
        }

        // ── Inner loop — play chunks sequentially until video ends ──
        int current = 0;
        bool had_error = false;

        while (streaming_thread_running) {
          int next = current + 1;

          // Kick off prefetch of next chunk while current plays
          std::atomic<bool> prefetch_success{false};
          prefetch_thread = std::thread([this, &url, next, &prefetch_success]() {
            try {
              prefetch_success = download_and_process_chunk(url, next, false);
              spdlog::info("Prefetch chunk {}: {}", next,
                           prefetch_success.load() ? "ok" : "failed (end of video)");
            } catch (const std::exception &e) {
              spdlog::warn("Prefetch chunk {} exception: {}", next, e.what());
            }
          });

          // Play current chunk
          if (!play_chunk(current)) {
            if (prefetch_thread.joinable()) prefetch_thread.join();
            had_error = (state.load() == State::Error);
            break;
          }

          // Current chunk done — wait for prefetch
          if (prefetch_thread.joinable()) {
            spdlog::debug("Waiting for prefetch of chunk {}", next);
            prefetch_thread.join();
          }

          // Delete current chunk to free disk — but always keep chunk 0
          if (current > 0)
            cleanup_chunk(current);

          if (!prefetch_success) {
            // Video ended — clean up chunks > 0 and loop from start
            spdlog::info("Video ended — looping from chunk 0");
            cleanup_non_first_chunks();
            break; // breaks inner loop, outer loop replays from chunk 0
          }

          current = next;
        }

        if (had_error) break;
      }

      streaming_thread_running = false;
      if (state.load() != State::Error) {
        state = State::Idle;
        send_websocket_message("status:idle");
      }
    } catch (const std::exception &e) {
      spdlog::error("Critical exception in streaming thread: {}", e.what());
      last_error = std::string("Streaming error: ") + e.what();
      state = State::Error;
      send_websocket_message("status:error");
      streaming_thread_running = false;
    } catch (...) {
      spdlog::error("Unknown exception in streaming thread");
      last_error = "Unknown streaming error";
      state = State::Error;
      send_websocket_message("status:error");
      streaming_thread_running = false;
    }
  });
}

void VideoDesktop::stop_stream() {
  streaming_thread_running = false;
  stop_audio();

  // Let processing_thread clean up prefetch_thread, then wait for it.
  if (processing_thread.joinable()) {
    try {
      processing_thread.join();
    } catch (const std::exception &e) {
      spdlog::error("Exception joining processing thread: {}", e.what());
    }
  }
  // Belt-and-suspenders in case processing_thread exited before joining prefetch
  if (prefetch_thread.joinable())
    prefetch_thread.join();

  if (stream_pipe) {
    close_pipe(stream_pipe);
    stream_pipe = nullptr;
  }

  current_frame_data.clear();
  state = State::Idle;
}

// ─── Audio ───────────────────────────────────────────────────────────────────

void VideoDesktop::start_audio(const std::string &path) {
  stop_audio();
#ifdef _WIN32
  std::string cmd = fmt::format("ffplay -nodisp -autoexit -loglevel error \"{}\"", path);
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  std::vector<char> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back('\0');
  if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    spdlog::error("Failed to start ffplay (error {})", GetLastError());
    return;
  }
  audio_process_info = pi;
#else
  audio_pid = fork();
  if (audio_pid == 0) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "error",
           path.c_str(), nullptr);
    _exit(1);
  } else if (audio_pid < 0) {
    spdlog::error("Failed to fork for audio playback");
    audio_pid = -1;
  }
#endif
}

void VideoDesktop::stop_audio() {
#ifdef _WIN32
  if (audio_process_info.hProcess) {
    if (WaitForSingleObject(audio_process_info.hProcess, 0) == WAIT_TIMEOUT)
      TerminateProcess(audio_process_info.hProcess, 0);
    CloseHandle(audio_process_info.hProcess);
    CloseHandle(audio_process_info.hThread);
    audio_process_info = {};
  }
#else
  if (audio_pid > 0) {
    kill(audio_pid, SIGTERM);
    int status;
    if (waitpid(audio_pid, &status, WNOHANG) == 0) {
      kill(audio_pid, SIGKILL);
      waitpid(audio_pid, &status, 0);
    }
    audio_pid = -1;
  }
  if (audio_pipe) {
    close_pipe(audio_pipe);
    audio_pipe = nullptr;
  }
#endif
}

// ─── Chunk download / process ─────────────────────────────────────────────────

bool VideoDesktop::download_and_process_chunk(const std::string &url,
                                              int chunk_index,
                                              bool set_error_on_fail) {
  try {
    const int start_sec = chunk_index * chunk_duration_sec;
    const int end_sec   = start_sec + chunk_duration_sec;

    std::string mp4Path = chunk_mp4_path(chunk_index);
    std::string binPath = chunk_bin_path(chunk_index);

    // Skip if the processed binary already exists (e.g. cached chunk 0)
    std::error_code ec;
    if (std::filesystem::exists(binPath, ec) &&
        std::filesystem::file_size(binPath, ec) > 0) {
      spdlog::info("Chunk {} already on disk, skipping download", chunk_index);
      return true;
    }

    // Download via yt-dlp
    std::string dlCmd = fmt::format(
        "yt-dlp -f \"best[ext=mp4]/best\" --download-sections \"*{}-{}\" "
        "--force-overwrites -o \"{}\" \"{}\"",
        start_sec, end_sec, mp4Path, url);
    spdlog::info("Downloading chunk {} ({}-{}s)", chunk_index, start_sec, end_sec);
    int dlResult = run_command(dlCmd);
    if (dlResult != 0) {
      std::string msg = fmt::format("yt-dlp chunk {} failed (exit {})", chunk_index, dlResult);
      if (set_error_on_fail) {
        last_error = msg;
        spdlog::error(last_error);
        state = State::Error;
        send_websocket_message("status:error");
      } else {
        spdlog::info("{} — video likely ended", msg);
      }
      return false;
    }

    // Process to raw RGB via ffmpeg
    std::string ffCmd = fmt::format(
        "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
        "-f rawvideo -pix_fmt rgb24 \"{}\"",
        mp4Path, matrix_width, matrix_height, fps, binPath);
    spdlog::info("Processing chunk {} to {}x{} @ {}fps", chunk_index,
                 matrix_width, matrix_height, fps);
    int ffResult = run_command(ffCmd);
    if (ffResult != 0) {
      std::string msg = fmt::format("ffmpeg chunk {} failed (exit {})", chunk_index, ffResult);
      if (set_error_on_fail) {
        last_error = msg;
        spdlog::error(last_error);
        state = State::Error;
        send_websocket_message("status:error");
      } else {
        spdlog::warn("{}", msg);
      }
      return false;
    }

    // ffmpeg can exit 0 but produce an empty file when the video segment has no frames
    // (e.g. yt-dlp downloaded a stub past the end of the video)
    std::error_code ec2;
    if (std::filesystem::file_size(binPath, ec2) == 0) {
      std::filesystem::remove(binPath, ec2);
      std::string msg = fmt::format("ffmpeg chunk {} produced empty output — video ended", chunk_index);
      if (set_error_on_fail) {
        last_error = msg;
        spdlog::error(last_error);
        state = State::Error;
        send_websocket_message("status:error");
      } else {
        spdlog::info("{}", msg);
      }
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    spdlog::error("Exception in download_and_process_chunk({}): {}", chunk_index, e.what());
    if (set_error_on_fail) {
      last_error = std::string("Chunk error: ") + e.what();
      state = State::Error;
      send_websocket_message("status:error");
    }
    return false;
  }
}

// ─── Paths ───────────────────────────────────────────────────────────────────

std::string VideoDesktop::get_video_id(const std::string &url) const {
  return std::to_string(std::hash<std::string>{}(url));
}

std::string VideoDesktop::chunk_mp4_path(int chunk_index) const {
  auto dir = get_data_dir() / "cache" / "video" / get_video_id(current_url);
  std::filesystem::create_directories(dir);
  return (dir / fmt::format("chunk_{}.mp4", chunk_index)).string();
}

std::string VideoDesktop::chunk_bin_path(int chunk_index) const {
  auto dir = get_data_dir() / "cache" / "video" / get_video_id(current_url);
  std::filesystem::create_directories(dir);
  return (dir / fmt::format("chunk_{}.bin", chunk_index)).string();
}

// ─── Cache management ─────────────────────────────────────────────────────────

void VideoDesktop::cleanup_chunk(int chunk_index) {
  if (chunk_index <= 0) return; // Never delete chunk 0
  std::error_code ec;
  std::filesystem::remove(chunk_mp4_path(chunk_index), ec);
  std::filesystem::remove(chunk_bin_path(chunk_index), ec);
}

void VideoDesktop::cleanup_non_first_chunks() {
  auto cacheDir = get_data_dir() / "cache" / "video" / get_video_id(current_url);
  if (!std::filesystem::exists(cacheDir)) return;

  auto keep_mp4 = std::filesystem::path(chunk_mp4_path(0)).filename();
  auto keep_bin = std::filesystem::path(chunk_bin_path(0)).filename();

  std::error_code ec;
  for (auto &entry : std::filesystem::directory_iterator(cacheDir, ec)) {
    auto name = entry.path().filename();
    if (name == keep_mp4 || name == keep_bin) continue;
    if (entry.path().extension() == ".mp4" || entry.path().extension() == ".bin")
      std::filesystem::remove(entry, ec);
  }
}

void VideoDesktop::evict_oldest_first_chunks() {
  auto videoRoot = get_data_dir() / "cache" / "video";
  std::error_code ec;
  if (!std::filesystem::exists(videoRoot, ec)) return;

  std::string currentId = get_video_id(current_url);

  // Collect other videos' folders sorted by age (oldest first)
  std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> others;
  for (auto &entry : std::filesystem::directory_iterator(videoRoot, ec)) {
    if (!entry.is_directory(ec)) continue;
    if (entry.path().filename() == currentId) continue;
    others.emplace_back(entry.last_write_time(ec), entry.path());
  }

  // Allow MAX_FIRST_CHUNK_CACHE - 1 other folders (current counts as 1)
  int allowed = MAX_FIRST_CHUNK_CACHE - 1;
  if (static_cast<int>(others.size()) <= allowed) return;

  std::sort(others.begin(), others.end()); // oldest first
  int excess = static_cast<int>(others.size()) - allowed;
  for (int i = 0; i < excess; ++i) {
    spdlog::info("Evicting old video cache: {}", others[i].second.string());
    std::filesystem::remove_all(others[i].second, ec);
  }
}

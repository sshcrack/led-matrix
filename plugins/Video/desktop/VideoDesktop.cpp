#include "VideoDesktop.h"
#include "VideoPacket.h"
#include "shared/desktop/utils.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <stdexcept>
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

inline FILE *open_pipe(const char *cmd) {
#ifdef _WIN32
  return _popen(cmd, "rb");
#else
  return popen(cmd, "r");
#endif
}

inline void close_pipe(FILE *pipe) {
  if (!pipe)
    return;
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
}

inline int run_command_no_window(const std::string &cmd) {
#ifdef _WIN32
  // Suppress spawning a console window for ffmpeg/yt-dlp.
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  std::string fullCmd = "cmd.exe /C " + cmd;
  std::vector<char> cmdline(fullCmd.begin(), fullCmd.end());
  cmdline.push_back('\0');

  if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    return -1;
  }

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
}

VideoDesktop::VideoDesktop() {
  // check_tools(); // Post-init is better for this
}

VideoDesktop::~VideoDesktop() {
  stop_stream();
}

void VideoDesktop::check_tools() {
  try {
    // Check ffmpeg
    const auto ffmpeg_cmd =
        fmt::format("ffmpeg -version > {} 2>&1", null_device());
    int ffmpeg_ret = run_command_no_window(ffmpeg_cmd);
    if (ffmpeg_ret != 0) {
      tools_available = false;
      tools_error_msg = "ffmpeg not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }

    // Check yt-dlp
    const auto ytdlp_cmd =
        fmt::format("yt-dlp --version > {} 2>&1", null_device());
    int ytdlp_ret = run_command_no_window(ytdlp_cmd);
    if (ytdlp_ret != 0) {
      tools_available = false;
      tools_error_msg = "yt-dlp not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }

    // Check ffplay for audio playback
    const auto ffplay_cmd =
        fmt::format("ffplay -version > {} 2>&1", null_device());
    int ffplay_ret = run_command_no_window(ffplay_cmd);
    if (ffplay_ret != 0) {
      tools_available = false;
      tools_error_msg = "ffplay not found in PATH (needed for audio playback).";
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
  if (!std::filesystem::exists(cacheDir)) {
    std::filesystem::create_directories(cacheDir);
  }
}

void VideoDesktop::load_config(std::optional<const nlohmann::json> config) {
  if (config.has_value()) {
    const auto& cfg = config.value();
    if (cfg.contains("enable_audio")) {
      enable_audio = cfg["enable_audio"].get<bool>();
    }
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
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s",
                       tools_error_msg.c_str());
    ImGui::Text("Video plugin functionality is disabled.");
    return;
  }

  render_status_ui();
}

void VideoDesktop::render_status_ui() {
  ImGui::Text("Current URL: %s",
              current_url.empty() ? "None" : current_url.c_str());

  if (ImGui::Checkbox("Enable Audio", &enable_audio)) {
    if (!enable_audio) {
      stop_audio();
    }
  }

  std::string stateStr = "Unknown";
  ImVec4 stateColor = ImVec4(1, 1, 1, 1);

  switch (state.load()) {
  case State::Idle:
    stateStr = "Idle";
    break;
  case State::Downloading:
    stateStr = "Downloading...";
    stateColor = ImVec4(1, 1, 0, 1);
    break;
  case State::Processing:
    stateStr = "Processing...";
    stateColor = ImVec4(1, 1, 0, 1);
    break;
  case State::Playing:
    stateStr = "Playing";
    stateColor = ImVec4(0, 1, 0, 1);
    break;
  case State::Finished:
    stateStr = "Finished";
    stateColor = ImVec4(0, 0.8, 0.8, 1);
    break;
  case State::Error:
    stateStr = "Error";
    stateColor = ImVec4(1, 0, 0, 1);
    break;
  }

  ImGui::TextColored(stateColor, "Status: %s", stateStr.c_str());

  if (state.load() == State::Error) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s",
                       last_error.c_str());
  }

  if (!status_message.empty()) {
    ImGui::Text("Info: %s", status_message.c_str());
  }
}

void VideoDesktop::pre_new_frame() {
  // Nothing to do; frames arrive via streaming thread.
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
VideoDesktop::compute_next_packet(const std::string sceneName) {
  if (sceneName != "video" || state.load() != State::Playing ||
      !tools_available || !allow_sending_packets.load()) {
    return std::nullopt;
  }

  // Throttle to target fps
  const auto now = std::chrono::steady_clock::now();
  const double frame_interval = 1.0 / fps;
  if (std::chrono::duration<double>(now - last_packet_time).count() <
      frame_interval) {
    return std::nullopt;
  }
  last_packet_time = now;

  std::lock_guard<std::mutex> lock(data_mutex);
  if (current_frame_data.empty()) {
    return std::nullopt;
  }

  return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(
      new VideoPacket(current_frame_data),
      [](UdpPacket *packet) { delete dynamic_cast<VideoPacket *>(packet); });
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
    
    // Always abort current stream when new URL comes in
    if (state.load() != State::Idle) {
      spdlog::info("Aborting current stream for new URL: {}", newUrl);
      stop_stream();
      spdlog::info("Current stream stopped.");
    }

    current_url = newUrl;
    start_stream(newUrl);
  }

  if (message.starts_with("size:")) {
    std::string sizeStr = message.substr(5);
    const auto xPos = sizeStr.find('x');

    matrix_width = std::stoi(sizeStr.substr(0, xPos));
    matrix_height = std::stoi(sizeStr.substr(xPos + 1));
  }
}

// Helpers
std::string VideoDesktop::get_video_id(const std::string &url) const {
  // Simple hash for file naming
  size_t hash = std::hash<std::string>{}(url);
  return std::to_string(hash);
}
void VideoDesktop::start_stream(const std::string &url) {
  stop_stream();

  state = State::Downloading;
  spdlog::info("Starting stream for URL: {}", url);
  send_websocket_message("status:downloading");

  // Reset pacing so first frame can go out immediately after start
  last_packet_time = std::chrono::steady_clock::now();

  streaming_thread_running = true;
  processing_thread = std::thread([this, url]() {
    const size_t frameSize = static_cast<size_t>(matrix_width) *
                             static_cast<size_t>(matrix_height) * 3;

    auto play_chunk = [&](int chunk_index) -> bool {
      spdlog::info("Playing chunk {}", chunk_index);
      std::string mp4Path = chunk_mp4_path(chunk_index);
      std::string binPath = chunk_bin_path(chunk_index);
      FILE *bin = fopen(binPath.c_str(), "rb");
      if (!bin) {
        last_error = "Failed to open processed chunk: " + binPath;
        state = State::Error;
        send_websocket_message("status:error");
        return false;
      }

      if (enable_audio) {
        start_audio(mp4Path);
      }

      state = State::Playing;
      send_websocket_message("status:playing");

      std::vector<uint8_t> buffer(frameSize);
      size_t frame_idx = 0;
      const size_t frames_per_chunk = static_cast<size_t>(chunk_duration_sec * fps);

      while (streaming_thread_running) {
        size_t readBytes = fread(buffer.data(), 1, frameSize, bin);
        if (readBytes == frameSize) {
          {
            std::lock_guard<std::mutex> lock(data_mutex);
            current_frame_data = buffer;
          }

          // Pace playback
          const auto frame_interval = std::chrono::duration<double>(1.0 / fps);
          std::this_thread::sleep_for(frame_interval);
          frame_idx++;

          if (frame_idx >= frames_per_chunk) {
            break; // move to next chunk
          }
        } else {
          break; // end of chunk file
        }
      }

      fclose(bin);
      stop_audio();
      return streaming_thread_running.load();
    };

    // Initial chunk 0
    spdlog::info("Downloading and processing initial chunk 0");
    if (!download_and_process_chunk(url, 0)) {
      streaming_thread_running = false;
      return;
    }

    current_chunk = 0;

    spdlog::info("Starting playback loop");
    while (streaming_thread_running) {
      // Start prefetching next chunk in background
      int next_chunk = current_chunk + 1;
      spdlog::info("Starting prefetch of chunk {} in background", next_chunk);
      
      std::atomic<bool> prefetch_success{false};
      prefetch_thread = std::thread([this, url, next_chunk, &prefetch_success]() {
        if (download_and_process_chunk(url, next_chunk, false)) {
          spdlog::info("Prefetch of chunk {} completed successfully", next_chunk);
          prefetch_success = true;
        } else {
          spdlog::info("Prefetch of chunk {} failed (video ended)", next_chunk);
          prefetch_success = false;
        }
      });

      // Play current chunk while prefetch happens in background
      if (!play_chunk(current_chunk.load())) {
        break;
      }

      // Wait for prefetch to complete after playback finishes
      if (prefetch_thread.joinable()) {
        spdlog::info("Waiting for prefetch of chunk {} to complete", next_chunk);
        prefetch_thread.join();
      }

      // If prefetch failed, video is done (shorter than expected)
      if (!prefetch_success) {
        spdlog::info("Video finished - no more chunks available");
        break;
      }

      // Clean up the chunk that's two steps behind
      cleanup_chunk(current_chunk - 1);

      // Move to next chunk
      current_chunk = next_chunk;
    }

    streaming_thread_running = false;
    state = State::Finished;
    send_websocket_message("status:finished");
  });
}

void VideoDesktop::stop_stream() {
  streaming_thread_running = false;
  stop_audio();
  if (prefetch_thread.joinable()) {
    prefetch_thread.join();
  }
  if (stream_pipe) {
    close_pipe(stream_pipe);
    stream_pipe = nullptr;
  }
  if (processing_thread.joinable()) {
    processing_thread.join();
  }
  current_frame_data.clear();
  state = State::Idle;
}

void VideoDesktop::start_audio(const std::string &path) {
  stop_audio();

#ifdef _WIN32
  std::string cmd =
      fmt::format("ffplay -nodisp -autoexit -loglevel error \"{}\"", path);

  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  std::vector<char> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back('\0');

  if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    spdlog::error("Failed to start ffplay for audio (error code {})",
                  GetLastError());
    return;
  }

  audio_process_info = pi;
#else
  // Fork a child process to run ffplay
  audio_pid = fork();
  if (audio_pid == 0) {
    // Child process
    // Redirect stdout and stderr to /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    
    // Execute ffplay
    execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "error",
           path.c_str(), nullptr);
    
    // If execlp fails
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
    // If still running, terminate; otherwise just close handles.
    if (WaitForSingleObject(audio_process_info.hProcess, 0) == WAIT_TIMEOUT) {
      TerminateProcess(audio_process_info.hProcess, 0);
    }

    CloseHandle(audio_process_info.hProcess);
    CloseHandle(audio_process_info.hThread);
    audio_process_info = {};
  }
#else
  if (audio_pid > 0) {
    // Send SIGTERM to the process
    kill(audio_pid, SIGTERM);
    
    // Wait briefly for graceful shutdown
    int status;
    pid_t result = waitpid(audio_pid, &status, WNOHANG);
    
    // If process didn't exit, force kill it
    if (result == 0) {
      // Still running, force kill
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

bool VideoDesktop::download_and_process_chunk(const std::string &url,
                                              int chunk_index, bool set_error_on_fail) {
  const int start_sec = chunk_index * chunk_duration_sec;
  const int end_sec = start_sec + chunk_duration_sec;

  std::string mp4Path = chunk_mp4_path(chunk_index);
  std::string binPath = chunk_bin_path(chunk_index);

  // Download chunk
  std::string dlCmd = fmt::format(
      "yt-dlp -f \"best[ext=mp4]/best\" --remote-components ejs:npm "
      "--download-sections \"*{}-{}\" --force-overwrites -o \"{}\" \"{}\"",
      start_sec, end_sec, mp4Path, url);

  spdlog::info("Downloading chunk {} ({}-{}s): {}", chunk_index, start_sec,
               end_sec, dlCmd);
  if (run_command_no_window(dlCmd) != 0) {
    if (set_error_on_fail) {
      last_error = "yt-dlp chunk download failed";
      spdlog::error(last_error);
      state = State::Error;
      send_websocket_message("status:error");
    } else {
      spdlog::warn("Chunk {} download failed (video may be shorter than expected)", chunk_index);
    }
    return false;
  }

  // Process chunk to raw
  std::string ffCmd = fmt::format(
      "ffmpeg -y -i \"{}\" -vf \"scale={}:{} ,setsar=1:1,fps={}\" -f "
      "rawvideo -pix_fmt rgb24 \"{}\"", mp4Path, matrix_width,
      matrix_height, fps, binPath);

  spdlog::info("Processing chunk {}: {}", chunk_index, ffCmd);
  if (run_command_no_window(ffCmd) != 0) {
    if (set_error_on_fail) {
      last_error = "ffmpeg chunk processing failed";
      spdlog::error(last_error);
      state = State::Error;
      send_websocket_message("status:error");
    } else {
      spdlog::warn("Chunk {} processing failed (video may be shorter than expected)", chunk_index);
    }
    return false;
  }

  return true;
}

std::string VideoDesktop::chunk_mp4_path(int chunk_index) const {
  auto cacheDir = get_data_dir() / "cache" / "video" /
                  get_video_id(current_url);
  if (!std::filesystem::exists(cacheDir)) {
    std::filesystem::create_directories(cacheDir);
  }
  return (cacheDir / fmt::format("chunk_{}.mp4", chunk_index)).string();
}

std::string VideoDesktop::chunk_bin_path(int chunk_index) const {
  auto cacheDir = get_data_dir() / "cache" / "video" /
                  get_video_id(current_url);
  if (!std::filesystem::exists(cacheDir)) {
    std::filesystem::create_directories(cacheDir);
  }
  return (cacheDir / fmt::format("chunk_{}.bin", chunk_index)).string();
}

void VideoDesktop::cleanup_chunk(int chunk_index) {
  if (chunk_index < 0)
    return;
  std::error_code ec;
  std::filesystem::remove(chunk_mp4_path(chunk_index), ec);
  std::filesystem::remove(chunk_bin_path(chunk_index), ec);
}

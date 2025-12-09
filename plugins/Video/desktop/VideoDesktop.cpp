#include "VideoDesktop.h"
#include "VideoPacket.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <imgui.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo() {
  return new VideoDesktop();
}

extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c) { delete c; }

VideoDesktop::VideoDesktop() {
  // check_tools(); // Post-init is better for this
}

VideoDesktop::~VideoDesktop() {
  processing_thread_running = false;
  if (processing_thread.joinable()) {
    processing_thread.join();
  }
}

// Utility to run command and get output
std::string exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

void VideoDesktop::check_tools() {
  try {
    // Check ffmpeg
    int ffmpeg_ret = system("ffmpeg -version > /dev/null 2>&1");
    if (ffmpeg_ret != 0) {
      tools_available = false;
      tools_error_msg = "ffmpeg not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }

    // Check yt-dlp
    int ytdlp_ret = system("yt-dlp --version > /dev/null 2>&1");
    if (ytdlp_ret != 0) {
      tools_available = false;
      tools_error_msg = "yt-dlp not found in PATH.";
      spdlog::error(tools_error_msg);
      return;
    }

    tools_available = true;
    spdlog::info("ffmpeg and yt-dlp found.");

  } catch (const std::exception &e) {
    tools_available = false;
    tools_error_msg = "Error checking tools: " + std::string(e.what());
    spdlog::error(tools_error_msg);
  }
}

void VideoDesktop::post_init() {
  check_tools();

  auto cacheDir =
      _plugin_location.parent_path().parent_path() / "cache" / "video";
  if (!std::filesystem::exists(cacheDir)) {
    std::filesystem::create_directories(cacheDir);
  }
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
  // Only fetch frames if we are playing
  if (state.load() == State::Playing) {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = now - last_frame_time;

    if (diff.count() >= (1.0 / fps)) {
      // Need new frame
      if (video_file_stream.is_open()) {
        size_t frameSize = matrix_width * matrix_height * 3;
        std::vector<uint8_t> frame(frameSize);

        if (video_file_stream.read(reinterpret_cast<char *>(frame.data()),
                                   frameSize)) {
          std::lock_guard<std::mutex> lock(data_mutex);
          current_frame_data = frame;
          last_frame_time = now;
        } else {
          // Start over? OR stop? For now loop
          video_file_stream.clear();
          video_file_stream.seekg(0);
          if (video_file_stream.read(reinterpret_cast<char *>(frame.data()),
                                     frameSize)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            current_frame_data = frame;
            last_frame_time = now;
          } else {
            // Error reading file
            state = State::Error;
            last_error = "Could not read processed video file.";
            send_websocket_message("status:error");
          }
        }
      } else {
        std::filesystem::path processedPath = get_processed_path(current_url);
        video_file_stream.open(processedPath, std::ios::binary);
        if (!video_file_stream.is_open()) {
          state = State::Error;
          last_error = "Failed to open processed video file.";
          send_websocket_message("status:error");
        }
      }
    }
  }
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
VideoDesktop::compute_next_packet(const std::string sceneName) {
  if (sceneName != "video" || state.load() != State::Playing ||
      !tools_available) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(data_mutex);
  if (current_frame_data.empty()) {
    return std::nullopt;
  }

  return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(
      new VideoPacket(current_frame_data),
      [](UdpPacket *packet) { delete dynamic_cast<VideoPacket *>(packet); });
}

void VideoDesktop::on_websocket_message(const std::string message) {
  if (message.starts_with("url:")) {
    if (!tools_available) {
      send_websocket_message("status:error");
      return;
    }

    std::string newUrl = message.substr(4);
    if (newUrl == current_url && state.load() == State::Playing) {
      return; // Already playing this
    }

    current_url = newUrl;

    // Stop previous work logic
    processing_thread_running = false;
    if (processing_thread.joinable()) {
      processing_thread.join();
    }

    // Start new thread
    processing_thread_running = true;
    processing_thread = std::thread([this, newUrl]() {
      // Check if already processed
      std::filesystem::path processedPath = get_processed_path(newUrl);

      if (std::filesystem::exists(processedPath)) {
        // Already cached
        spdlog::info("Video already cached: {}", newUrl);
        state = State::Playing;
        send_websocket_message("status:playing");

        std::lock_guard<std::mutex> lock(data_mutex);
        if (video_file_stream.is_open())
          video_file_stream.close();
        // Reset file stream in main thread or here?
        // It's safer to flag it and let pre_new_frame handle opening if
        // possible, but pre_new_frame runs on main thread. We need to
        // synchronize. We will close it here, and pre_new_frame will open it.
      } else {
        // Determine if it is a local file or download
        // Simple heuristic: if it starts with http/https, try to
        // download/process However, user said "youtube video urls or general
        // mp4 urls" yt-dlp handles many urls. logic: try yt-dlp.

        state = State::Downloading;
        send_websocket_message("status:downloading");

        if (download_video(newUrl)) {
          state = State::Processing;
          send_websocket_message("status:processing");

          std::string cacheFile = get_cache_path(newUrl);
          if (process_video(cacheFile)) {
            state = State::Playing;
            send_websocket_message("status:playing");
            if (video_file_stream.is_open())
              video_file_stream.close();
          } else {
            state = State::Error;
            send_websocket_message("status:error");
          }
        } else {
          state = State::Error;
          send_websocket_message("status:error");
        }
      }
    });
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

std::string VideoDesktop::get_cache_path(const std::string &url) const {
  auto cacheDir =
      _plugin_location.parent_path().parent_path() / "cache" / "video";
  return (cacheDir / (get_video_id(url) + "_download.mp4")).string();
}

std::string VideoDesktop::get_processed_path(const std::string &url) const {
  auto cacheDir =
      _plugin_location.parent_path().parent_path() / "cache" / "video";
  return (cacheDir / (get_video_id(url) + "_processed.bin")).string();
}

bool VideoDesktop::download_video(const std::string &url) {
  if (!tools_available)
    return false;

  std::string outputPath = get_cache_path(url);

  // If it is a direct file url ending in .mp4, we might just wget it?
  // But yt-dlp is robust.

  // Command: yt-dlp -f
  // "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best" -o <output>
  // <url> Note: for LED matrix we don't strictly need audio, but usually mp4 is
  // fine. Let's force mp4.

  std::string cmd =
      "yt-dlp -f \"best[ext=mp4]/best\" --force-overwrites -o \"" + outputPath +
      "\" \"" + url + "\"";
  spdlog::info("Executing: {}", cmd);

  int result = system(cmd.c_str());
  if (result != 0) {
    last_error = "yt-dlp failed to download video.";
    spdlog::error(last_error);
    return false;
  }
  return true;
}

bool VideoDesktop::process_video(const std::string &inputFile) {
  if (!tools_available)
    return false;

  std::string outputPath = get_processed_path(current_url);

  // ffmpeg command to scale and output raw RGB24
  // -i input -vf scale=128:128 -f rawvideo -pix_fmt rgb24 output.bin

  std::string cmd =
      fmt::format("ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1\" -f "
                  "rawvideo -pix_fmt rgb24 \"{}\"",
                  inputFile, matrix_width, matrix_height, outputPath);

  spdlog::info("Executing: {}", cmd);

  int result = system(cmd.c_str());
  if (result != 0) {
    last_error = "ffmpeg failed to process video.";
    spdlog::error(last_error);
    return false;
  }
  return true;
}

#pragma once
#include "shared/desktop/plugin/main.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

using namespace Plugins;

class VideoDesktop final : public DesktopPlugin {
public:
  VideoDesktop();
  ~VideoDesktop() override;

  void render() override;
  void initialize_imgui(ImGuiContext *im_gui_context,
                        ImGuiMemAllocFunc *alloc_fn, ImGuiMemFreeFunc *free_fn,
                        void **user_data) override;
  void pre_new_frame() override;
  void post_init() override;
  void load_config(std::optional<const nlohmann::json> config) override;
  void save_config(nlohmann::json &config) const override;

  std::string get_plugin_name() const override { return PLUGIN_NAME; }

  std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
  compute_next_packet(const std::string sceneName) override;

  void on_websocket_message(const std::string message) override;

private:
  void check_tools();
  void render_status_ui();
  void start_stream(const std::string &url);
  void stop_stream();
  void start_audio(const std::string &path);
  void stop_audio();
  bool download_and_process_chunk(const std::string &url, int chunk_index, bool set_error_on_fail = true);
  std::string chunk_mp4_path(int chunk_index) const;
  std::string chunk_bin_path(int chunk_index) const;
  void cleanup_chunk(int chunk_index);

  std::string get_video_id(const std::string &url) const;

  // State
  bool tools_available = false;
  std::string tools_error_msg;

  std::string current_url;
  bool enable_audio = true;
  int matrix_width = 128;
  int matrix_height = 128;

  enum class State { Idle, Downloading, Processing, Playing, Finished, Error };
  std::atomic<State> state = State::Idle;
  std::string last_error;
  std::string status_message;
  std::atomic<bool> allow_sending_packets{true};
  std::atomic<bool> streaming_thread_running{false};
  FILE *stream_pipe = nullptr;

#ifdef _WIN32
  PROCESS_INFORMATION audio_process_info{};
#else
  FILE *audio_pipe = nullptr;
  pid_t audio_pid = -1;
#endif

  // Chunked streaming
  const int chunk_duration_sec = 300;      // 5 minutes per chunk
  std::atomic<int> current_chunk{0};
  std::thread prefetch_thread;

  // Playback
  std::vector<uint8_t> current_frame_data;
  std::mutex data_mutex;
  std::chrono::steady_clock::time_point last_packet_time =
      std::chrono::steady_clock::now();

  std::thread processing_thread;

  // Frame pacing (unused but kept for possible future throttling)
  double fps = 30.0;
};

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo();
extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c);

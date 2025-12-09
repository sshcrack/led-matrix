#pragma once
#include <fstream>

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

  std::string get_plugin_name() const override { return PLUGIN_NAME; }

  std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>>
  compute_next_packet(const std::string sceneName) override;

  void on_websocket_message(const std::string message) override;

private:
  void check_tools();
  void process_logic_loop();
  void render_status_ui();

  // Commands
  bool download_video(const std::string &url);
  bool process_video(const std::string &inputFile);

  // Helpers
  std::string get_cache_path(const std::string &url) const;
  std::string get_processed_path(const std::string &url) const;
  std::string get_video_id(const std::string &url) const;

  // State
  bool tools_available = false;
  std::string tools_error_msg;

  std::string current_url;
  int matrix_width = 128;
  int matrix_height = 128;

  enum class State { Idle, Downloading, Processing, Playing, Error };
  std::atomic<State> state = State::Idle;
  std::string last_error;
  std::string status_message;

  // Playback
  std::vector<uint8_t> current_frame_data;
  std::mutex data_mutex;

  std::thread processing_thread;
  std::atomic<bool> processing_thread_running = false;

  // Frame reading
  std::ifstream video_file_stream;
  std::chrono::steady_clock::time_point last_frame_time;
  double fps = 30.0;
};

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo();
extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c);

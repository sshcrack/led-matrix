#pragma once
#include "shared/desktop/plugin/main.h"
#include "shared/desktop/VideoStreamEngine.h"
#include <atomic>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Plugins;

class VideoDesktop final : public DesktopPlugin {
public:
  VideoDesktop() = default;
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
  void render_status_ui();
  void start_audio(const std::string &path);
  void stop_audio();

  // State
  bool tools_available = false;
  std::string tools_error_msg;
  std::string current_url;
  bool enable_audio = true;
  int matrix_width = 128;
  int matrix_height = 128;

  enum class State { Idle, Downloading, Playing, Error };
  std::atomic<State> state{State::Idle};
  std::string last_error;
  std::atomic<bool> allow_sending_packets{true};

#ifdef _WIN32
  PROCESS_INFORMATION audio_process_info{};
#else
  FILE *audio_pipe = nullptr;
  pid_t audio_pid = -1;
#endif

  double fps = 30.0;

  std::unique_ptr<Shared::VideoStreamEngine> engine_;
};

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo();
extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c);

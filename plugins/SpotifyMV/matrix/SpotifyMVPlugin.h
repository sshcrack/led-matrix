#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "shared/matrix/frame_stream.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/plugin/main.h"

class SpotifyMVPlugin final : public Plugins::BasicPlugin {
public:
  std::vector<std::unique_ptr<Plugins::SceneWrapper>>
    create_scenes() override;
  std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>>
    create_image_providers() override;

  bool on_udp_packet(uint8_t pluginId, const uint8_t* data, size_t size) override;
  std::optional<std::vector<std::string>> on_websocket_open() override;
  void on_websocket_message(const std::string& message) override;
  std::string get_plugin_name() const override { return PLUGIN_NAME; }
  [[nodiscard]] bool requires_single_desktop_producer() const override { return true; }
  [[nodiscard]] std::vector<std::string> get_runtime_input_ids() const override {
    return {std::string(RuntimeInputIds::SpotifyMVReady)};
  }

  /// Music-video frames from the desktop (top-down RGB888).
  [[nodiscard]] const FrameStream::Receiver& frames() const { return frames_; }

  std::string get_status() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
  }

  void flush_status();
  void clear_last_track_message();

  void set_last_track_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(track_msg_mutex_);
    last_track_message_ = msg;
  }

  [[nodiscard]] bool is_stale() const
  {
      const auto age = frames_.age_ms();
      return !age || *age > std::chrono::duration<double, std::milli>(kStaleTimeout).count();
  }

  static constexpr auto kStaleTimeout = std::chrono::seconds(5);

private:
    FrameStream::Receiver frames_{"SpotifyMV"};

    std::mutex status_mutex_;
    std::string status_ = "idle";
    bool tools_ready_ = false;
    std::string prepared_track_id_;
    bool first_frame_ready_ = false;

    std::mutex track_msg_mutex_;
    std::string last_track_message_;

    void publish_runtime_state();
};

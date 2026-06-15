#pragma once
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

class SpotifyMVPlugin final : public Plugins::BasicPlugin {
public:
  std::vector<std::unique_ptr<Plugins::SceneWrapper, void(*)(Plugins::SceneWrapper*)>>
    create_scenes() override;
  std::vector<std::unique_ptr<Plugins::ImageProviderWrapper, void(*)(Plugins::ImageProviderWrapper*)>>
    create_image_providers() override;

  bool on_udp_packet(uint8_t pluginId, const uint8_t* data, size_t size) override;
  std::optional<std::vector<std::string>> on_websocket_open() override;
  void on_websocket_message(const std::string& message) override;
  std::string get_plugin_name() const override { return PLUGIN_NAME; }

  std::vector<uint8_t> get_frame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return frame_;
  }

  std::string get_status() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
  }

  void flush_status();

  void set_last_track_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(track_msg_mutex_);
    last_track_message_ = msg;
  }

  [[nodiscard]] bool is_stale() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return std::chrono::steady_clock::now() - last_frame_time_ > kStaleTimeout;
  }

  static constexpr auto kStaleTimeout = std::chrono::seconds(5);

private:
  std::mutex frame_mutex_;
  std::vector<uint8_t> frame_;
  std::chrono::steady_clock::time_point last_frame_time_{};

  std::mutex status_mutex_;
  std::string status_ = "idle";

  std::mutex track_msg_mutex_;
  std::string last_track_message_;
};

extern "C" PLUGIN_EXPORT SpotifyMVPlugin* createSpotifyMV();
extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVPlugin*);

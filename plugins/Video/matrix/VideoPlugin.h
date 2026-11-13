#pragma once

#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

#include "shared/matrix/frame_stream.h"
#include "shared/matrix/plugin/main.h"

using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::SceneWrapper;
using Plugins::ShaderProviderWrapper;

class VideoPlugin final : public BasicPlugin {
public:
  vector<std::unique_ptr<SceneWrapper>>
  create_scenes() override;

  vector<
      std::unique_ptr<ImageProviderWrapper>>
  create_image_providers() override;

  vector<
      std::unique_ptr<ShaderProviderWrapper>>
  create_shader_providers() override;

  bool on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                     const size_t size) override;

  std::string get_plugin_name() const override { return PLUGIN_NAME; }
  [[nodiscard]] bool requires_single_desktop_producer() const override { return true; }

  std::unique_ptr<router_t>
  register_routes(std::unique_ptr<router_t> router) override;

  std::optional<std::vector<std::string>> on_websocket_open() override;
  void on_websocket_message(const std::string &message) override;

  // Transitions may prepare a new VideoScene before stopping the old one.
  // Only the scene that owns the current stream may change or stop it.
  void start_stream(const void* owner);
  void set_stream_url(const void* owner, const std::string& url);
  void stop_stream(const void* owner);

  /// Decoded video frames from the desktop (top-down RGB888).
  [[nodiscard]] const FrameStream::Receiver& frames() const { return frames_; }

  std::string get_status() {
    std::lock_guard<std::mutex> lock(statusMutex);
    return status;
  }
  void flush_status() {
    std::lock_guard<std::mutex> lock(statusMutex);
    status = "idle";
  }

  /// "Play now" request from the web UI; consumed once by the running scene.
  /// Requests expire so a click while another scene is showing does not
  /// start a video minutes later.
  std::optional<std::string> take_play_request() {
    std::lock_guard<std::mutex> lock(searchMutex);
    auto r = std::move(playRequest);
    playRequest.reset();
    if (r && std::chrono::steady_clock::now() - playRequestedAt > std::chrono::seconds(15))
      return std::nullopt;
    return r;
  }
  void set_play_request(std::string url) {
    std::lock_guard<std::mutex> lock(searchMutex);
    playRequest = std::move(url);
    playRequestedAt = std::chrono::steady_clock::now();
  }

private:
  std::mutex streamMutex;
  const void* streamOwner = nullptr;
  std::string streamUrl;
  enum class SearchState { Idle, Searching, Done, Error };

  nlohmann::json search_snapshot();

  std::mutex searchMutex;
  SearchState searchState = SearchState::Idle;
  std::string searchQuery;
  std::string searchError;
  nlohmann::json searchItems = nlohmann::json::array();
  std::chrono::steady_clock::time_point searchStarted{};
  std::optional<std::string> playRequest;
  std::chrono::steady_clock::time_point playRequestedAt{};

  FrameStream::Receiver frames_{"Video"};

  std::mutex statusMutex;
  std::string status = "idle";
};
